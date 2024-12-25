#include <glob/glob.h>
#include <glob/version.h>

#include <clipp.h>
#include <iostream>
#include <string>
#include <set>
#include <algorithm>

#include <ghc/fs_std.hpp>  // namespace fs = std::filesystem;   or   namespace fs = ghc::filesystem;

#include "monolithic_examples.h"


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

			public:
				// rough demo of custom code which triggers progress reports while the glob() scan proceeds.
				virtual filter_state_t filter(fs::path path, filter_state_t glob_says_pass, const filter_info_t &info) override {
					if (info.depth != previous_scan_depth) {
						previous_scan_depth = info.depth;
						glob_says_pass.do_report_progress = true;
					}
					if (info.search_spec_index != previous_path_searched_index) {
						previous_path_searched_index = info.search_spec_index;
						glob_says_pass.do_report_progress = true;
					}

					return glob_says_pass;
				}

				// progress callback: shows currently processed path, pass/reject status and progress/scan completion estimate.
				// Return `false` to abort the glob action.
				virtual bool progress_reporting(const progress_info_t &info) override {
					std::cout << info.search_spec_index << "/" << info.search_spec_count << ": " << info.current_path << "\n";
					return true;
				}

				// ------------

				my_glob_cfg(const fs::path& basepath, std::vector<std::string> pathnames, bool recursive_search = true)
					: glob::options(basepath, pathnames, recursive_search)
				{
				};
				my_glob_cfg(const fs::path& basepath, const std::string& pathname, bool recursive_search = true)
					: glob::options(basepath, pathname, recursive_search)
				{
					init_max_recursion_depth_set(recursive_search);
				};

				virtual ~my_glob_cfg() = default;
			};

			(void)test_wildcard_translate();

			my_glob_cfg spec(basepath, patterns, recursive);

			auto results = glob::glob(spec);
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
