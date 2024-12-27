#include <glob/glob.h>
#include <glob/version.h>

#include <clipp.h>
#include <iostream>
#include <string>
#include <set>
#include <algorithm>
#include <stdint.h>
#include <numeric>
#include <chrono>

#include <ghc/fs_std.hpp>  // namespace fs = std::filesystem;   or   namespace fs = ghc::filesystem;

#include "monolithic_examples.h"

// https://stackoverflow.com/questions/13772567/how-to-get-the-cpu-cycle-count-in-x86-64-from-c
// https://stackoverflow.com/questions/19719617/comparing-the-time-measured-results-produced-by-rdtsc-clock-and-c11-stdchron



static int test_wildcard_translate(void)
{
	int rv = EXIT_SUCCESS;

	// https://www.man7.org/linux/man-pages/man7/glob.7.html, https://www.man7.org/linux/man-pages/man3/fnmatch.3.html et al.
	std::vector<std::string> teststrings{
		"~/a/b/" ">" R"(\~/a/b/)",
		"ab*c" ">" "ab.*c",
		"ab?c" ">" "ab.c",
		"ab[1-9]" ">" "ab[1-9]",
		"ab[--z]" ">" R"(ab[\--z])",
		"ab[%--]" ">" R"(ab[%-\-])",
		"ab[!1-9]" ">" "ab[^1-9]",
		"ab[^1-9]" ">" R"(ab[\^1-9])",
		"ab[a-]" ">" R"(ab[a\-])",
		"ab[-a]" ">" R"(ab[\-a])",
		"*.html" ">" R"(.*\.html)",
		"[][!]" ">" R"([\]\[\!])",
		"[A-Fa-f0-9]" ">" R"([A-Fa-f0-9])",
		"[]-]" ">" R"([\]\-])",
		"[!]a-]" ">" R"([^\]a\-])",
		R"(a[[?*\])" ">" R"(a[\[\?\*\\])",
		R"(b[[?*\\])" ">" R"(b[\[\?\*\\])",
		"x(ab|cd|ef)" ">" R"(x\(ab\|cd\|ef\))",
		"?(ab|cd|ef)" ">" R"((ab|cd|ef)?)",
		"*(ab|cd|ef)" ">" R"((ab|cd|ef)*)",
		"@(ab|cd|ef)" ">" R"((ab|cd|ef))",
		"!(ab|cd|ef)" ">" R"((?!(ab|cd|ef)))",
		"**" ">" ".*",
	};
	for (const auto& s : teststrings) {
		size_t start = 0;
		size_t end = s.find(">");
		std::string src;
		std::string dst;

		if (end != std::string::npos) {
			src = s.substr(start, end - start);
			start = end + 1;
			dst = "((" + s.substr(start) + R"()|[\r\n])$)";
		}
		else {
			src = s;
			dst = "((" + s + R"()|[\r\n])$)";
		}

		auto ist = glob::translate_pattern(src);
		if (ist != dst) {
			std::cerr << "ERROR: translate(\"" << src << "\") --> \"" << ist << "\" instead of expected: \"" << dst << "\"\n";
			rv = EXIT_FAILURE;
		}
	}
	return rv;
}

static int test(void)
{
	int rv = test_wildcard_translate();

	std::vector<fs::path> testpaths{
		"C:\\users\\abcdef\\AppData\\Local\\Temp\\",
		"/home/user/.config/Cppcheck/Cppcheck-GUI.conf",
		"~/a/b/.config/Cppcheck/Cppcheck-GUI.conf",
		"~mcFarlane/.config/Cppcheck/Cppcheck-GUI.conf",
		"base/~mcFarlane/.config/Cppcheck/Cppcheck-GUI.conf",
		"Cppcheck/Cppcheck-GUI.conf",
		"../../Cppcheck/Cppcheck-GUI.conf",
		"..\\..\\Cppcheck\\Cppcheck-GUI.conf",
		"Z:\\Cppcheck\\Cppcheck-GUI.conf",
		"\\\\?:\\Cppcheck\\Cppcheck-GUI.conf",
		"./Cppcheck/Cppcheck-GUI.conf",
		"Cppcheck-GUI.conf",
		"./Cppcheck-GUI.conf"
	};

	for (const auto& p : testpaths) {
		std::cout << "Examining the path " << p << " through iterators gives\n";
		std::cout << p.root_directory() << " |RN " << p.root_name() << " |RP " << p.root_path() << " |PP " << p.parent_path() << " |FN " << p.filename() << " |EX " << p.extension() << " |ST " << p.stem() << " |0 " << *(p.begin()) << " |1 " << *(++p.begin()) << " ||\n";
		for (auto it = p.begin(); it != p.end(); ++it) {
			std::cout << *it << " | ";
		}
		std::cout << '\n';
	}

	return rv;
}


#if defined(BUILD_MONOLITHIC)
#define main     glob_standalone_main
#endif

int main(int argc, const char** argv)
{
	using namespace clipp;

	bool recursive = false;
	bool bfs_mode = false;
	std::vector<std::string> patterns;
	std::set<std::string> tags;
	std::string basepath;
	enum class mode { none, help, version, glob, test };
	mode selected = mode::none;

	auto options = (
		option("-r", "--recursive").set(recursive) % "Run glob recursively",
		repeatable(option("-i", "--input").set(selected, mode::glob) & values("patterns", patterns)) % "Patterns to match",
		option("-b", "--basepath").set(basepath) % "Base directory to glob in",
		option("--bfs").set(bfs_mode) % "BFS mode instead of (default) DFS"

	);
	auto cli = (
		(options
		| command("-h", "--help").set(selected, mode::help) % "Show this screen."
		| command("-t", "--test").set(selected, mode::test) % "Run the glob system tests."
		| command("-v", "--version").set(selected, mode::version) % "Show version."
		),
		any_other(patterns).set(selected, mode::glob)
	);

	auto help = [cli]()
	{
		std::cerr << make_man_page(cli, "glob")
			.prepend_section("DESCRIPTION", "    Run glob to find all the pathnames matching a specified pattern")
			.append_section("LICENSE", "    MIT");
	};

	parse(argc, argv, cli);
	switch (selected)
	{
	default:
	case mode::none:
	case mode::help:
		help();
		return EXIT_SUCCESS;

	case mode::test:
		return test();

	case mode::version:
		std::cout << "glob, version " << GLOB_VERSION << std::endl;
		return EXIT_SUCCESS;

	case mode::glob:
		break;
	}

	if (patterns.empty())
	{
		help();
		return EXIT_SUCCESS;
	}

	try
	{
		if (bfs_mode)
		{
#if 0
			// simple implementation; see the #else branch for a more advanced usage of glob()
			glob::options spec(basepath, patterns, recursive);

			auto results = glob::glob(spec);
			for (auto & match : results)
			{
				std::cout << match << "\n";
			}
#else
			class my_glob_cfg : public glob::options {
				int previous_scan_depth = -1;
				int previous_path_searched_index = -1;
				int previous_total_count_scanned_marker = -1;
				std::chrono::high_resolution_clock::time_point t_last;

			public:
				// rough demo of custom code which triggers progress reports while the glob() scan proceeds.
				virtual filter_state_t filter(fs::path path, filter_state_t glob_says_pass, const filter_info_t &info) override {
					int previous_total_count_marker = previous_total_count_scanned_marker;

					if (info.depth != previous_scan_depth) {
						previous_scan_depth = info.depth;
						glob_says_pass.do_report_progress = true;
					}
					if (info.actual_search_spec_index != previous_path_searched_index) {
						previous_path_searched_index = info.actual_search_spec_index;
						glob_says_pass.do_report_progress = true;
					}
					int total_count_scanned = info.dir_count_scanned + info.item_count_scanned;
					if (total_count_scanned >= previous_total_count_scanned_marker + 100) {
						previous_total_count_scanned_marker = total_count_scanned;
						glob_says_pass.do_report_progress = true;
					}

					typedef std::ratio<1, (int64_t)1e9> period; // @ 1.0 GHz
					auto end = std::chrono::high_resolution_clock::now();
					std::chrono::duration<double, period> diff = end - t_last;
					auto t = diff.count() / 1e9;   // ==> `t` is in seconds.
					if (t < 50e-3) {
						// do hammer the progress reporter too frequently!
						glob_says_pass.do_report_progress = false;
					}
					else {
						if (t >= 1) {
							// trigger a progress report every second at least once, no matter what!
							glob_says_pass.do_report_progress = true;
						}
					}

					if (glob_says_pass.do_report_progress) {
						t_last = end;
					}

					return glob_says_pass;
				}

				int previous_search_spec_count = 1;
				float files_per_dir_ema = 2e3;
				float previous_percentage = 0;

				// progress callback: shows currently processed path, pass/reject status and progress/scan completion estimate.
				// Return `false` to abort the glob action.
				virtual bool progress_reporting(const progress_info_t &info, const filter_state_t state) override {
					//std::cout << info.search_spec_index << "/" << info.search_spec_count << ": " << info.entry.path() << "\n";

					// By design:
					// *quadratic* progress rate (with extra climb_rate adjustment gradient) ensures we won't be nearing those 90%+ progress values very soon,
					// thus better managing the user's expectation, while we keep gathering more subdirectories-we-didn't-know-yet to scan before we will
					// have completed this glob() run.
					//
					// Meanwhile the climb_rate will be approaching 1.000 ever closer while we keep gathering subdirectories to scan, thus dropping that
					// rate correction from the equation as we near the 100% completion mark: running a BFS scan, we expect most of our last filter actions
					// to involve *files*, rather than *directories*, hence the spec_count should stabilize towards the end (is our assumption/expectation).
					//
					// Meanwhile, to improve the visual responsiveness and numbers, particularly in the first part of the glob() run, we also track an EMA
					// (exponential moving average) of the number of files per directory scanned, with an initial estimate of 1000. The actual ratio is
					// weighted against this EMA and used to adjust the progress percentage downwards while we have little actual information, i.e. when the
					// raw progress estimate is closer to 0 than 1 (100%).
					//
					float climb_rate = (info.search_spec_count + 0.0) / previous_search_spec_count;
					float progress = (info.actual_search_spec_index * 1.0 * info.actual_search_spec_index) / (info.search_spec_count * climb_rate * previous_search_spec_count);
					previous_search_spec_count = info.search_spec_count;

#if 0
					float fpd_ratio = (info.item_count_scanned + 1.0) / (info.dir_count_scanned + 1.0);
					const float ema_rate = 50.0;
					// tweaked EMA: jump-adjust when new value is higher, rather than lower that the current EMA: this makes this an EMA with peak detector built-in.
					files_per_dir_ema = ((ema_rate - 1) * std::max(files_per_dir_ema, fpd_ratio) + fpd_ratio) / ema_rate;
					float fpd_correction_factor = fpd_ratio / files_per_dir_ema;
					float impact = 1.0 - progress;
					impact *= impact;			// square to press impact towards the lower quarter of the progress range.
					float adjust = fpd_correction_factor * impact + 1.0 * (1.0 - impact); 
					float adjusted_progress = progress * adjust;

					std::cerr << std::format("{:7.3f}% ADJ:{:.7f} IMP:{:.7f} EMA:{:.7f} P:{:8.5f}%      ", adjusted_progress * 100.0, adjust, impact, files_per_dir_ema, progress * 100) << "\r";
#else
					// quadratic progress behaviour is not enough to deal with spurious progress jumps in the < 20% range the way we do below, so we 
					// give progress a stronger curve: cubic.
					progress *= (info.actual_search_spec_index * 1.0) / (info.search_spec_count);

					// due to some directories resulting in sudden jumps in downwards progress, we make it more human-friendly by
					// never showing this (it can surprise users), but instead keep moving the visible digits, i.e. increment the milli-percentage value.
					//
					// This should work out okay in actual practice as this behaviour generally occurs when progress is still in the lower tens of percent (0-30%).
					if (progress < previous_percentage) {
						progress = previous_percentage + 0.00001;
					}
					previous_percentage = progress;

					std::cerr << std::format("{:7.3f}% ITEMS:{} DIRS:{}      ", progress * 100.0, info.item_count_scanned, info.dir_count_scanned) << "\r";
#endif
					return true;
				}

				// ------------

				my_glob_cfg(const fs::path& basepath, std::vector<std::string> pathnames, bool recursive_search = true)
					: glob::options(basepath, pathnames, recursive_search)
				{
					t_last = std::chrono::high_resolution_clock::now();
					init_max_recursion_depth_set(recursive_search);
				};
				my_glob_cfg(const fs::path& basepath, const std::string& pathname, bool recursive_search = true)
					: glob::options(basepath, pathname, recursive_search)
				{
					t_last = std::chrono::high_resolution_clock::now();
					init_max_recursion_depth_set(recursive_search);
				};

				virtual ~my_glob_cfg() = default;
			};

			(void)test_wildcard_translate();

			my_glob_cfg spec(basepath, patterns, recursive);

			auto results = glob::glob(spec);
			std::cerr << "\n";
			for (auto & match : results)
			{
				std::cout << match << "\n";
			}
#endif
		}
		else
		{
			if (recursive)
			{
				if (!basepath.empty())
				{
					for (auto& match : glob::rglob_path(basepath, patterns))
					{
						std::cout << match << "\n";
					}
				} else
				{
					for (auto& match : glob::rglob(patterns))
					{
						std::cout << match << "\n";
					}
				}
			} else
			{
				if (!basepath.empty())
				{
					for (auto& match : glob::glob_path(basepath, patterns))
					{
						std::cout << match << "\n";
					}
				} else
				{
					for (auto& match : glob::glob(patterns))
					{
						std::cout << match << "\n";
					}
				}
			}
		}
	}
	catch (fs::filesystem_error &ex)
	{
		std::cerr << "glob/filesystem error " << ex.code().value() << ": " << ex.code().message() << " :: " << ex.what() << " (path: '" << ex.path1().string() << "')" << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
