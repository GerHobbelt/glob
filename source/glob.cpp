#include <glob/glob.h>

#include <cassert>

#include <algorithm>
#include <map>
#include <regex>
#include <string_view>
#include <format>

namespace glob {

	namespace {

		static constexpr auto SPECIAL_CHARACTERS = std::string_view{"()[]{}?*+-|^$\\.&~# \t\n\r\v\f"};
		static const auto ESCAPE_SET_OPER = std::regex(std::string{R"([&~|])"});
		static const auto ESCAPE_REPL_STR = std::string{R"(\\\1)"};

		bool string_replace(std::string &str, std::string_view from, std::string_view to) {
			std::size_t start_pos = str.find(from);
			if (start_pos == std::string::npos)
				return false;
			str.replace(start_pos, from.length(), to);
			return true;
		}

		std::string translate(std::string_view pattern) {
			std::size_t i = 0, n = pattern.size();
			std::string result_string;

			while (i < n) {
				auto c = pattern[i];
				i += 1;
				if (c == '*') {
					result_string += ".*";
				} 
				else if (c == '?') {
					result_string += ".";
				} 
				else if (c == '[') {
					auto j = i;
					if (j < n && pattern[j] == '!') {
						j += 1;
					}
					if (j < n && pattern[j] == ']') {
						j += 1;
					}
					while (j < n && pattern[j] != ']') {
						j += 1;
					}
					if (j >= n) {
						result_string += "\\[";
					} 
					else {
						auto stuff = std::string(pattern.begin() + i, pattern.begin() + j);
						if (stuff.find("--") == std::string::npos) {
							string_replace(stuff, std::string_view{"\\"}, std::string_view{R"(\\)"});
						} 
						else {
							std::vector<std::string> chunks;
							std::size_t k = 0;
							if (pattern[i] == '!') {
								k = i + 2;
							} 
							else {
								k = i + 1;
							}

							while (true) {
								k = pattern.find("-", k, j);
								if (k == std::string_view::npos) {
									break;
								}
								chunks.push_back(std::string(pattern.begin() + i, pattern.begin() + k));
								i = k + 1;
								k = k + 3;
							}

							chunks.push_back(std::string(pattern.begin() + i, pattern.begin() + j));
							// Escape backslashes and hyphens for set difference (--).
							// Hyphens that create ranges shouldn't be escaped.
							bool first = true;
							for (auto &chunk : chunks) {
								string_replace(chunk, std::string_view{"\\"}, std::string_view{R"(\\)"});
								string_replace(chunk, std::string_view{"-"}, std::string_view{R"(\-)"});
								if (first) {
									stuff += chunk;
									first = false;
								} 
								else {
									stuff += "-" + chunk;
								}
							}
						}

						// Escape set operations (&&, ~~ and ||).
						std::string result{};
						std::regex_replace(std::back_inserter(result), // result
															 stuff.begin(), stuff.end(), // string
															 ESCAPE_SET_OPER,            // pattern
															 ESCAPE_REPL_STR);           // repl
						stuff = result;
						i = j + 1;
						if (stuff[0] == '!') {
							stuff = "^" + std::string(stuff.begin() + 1, stuff.end());
						} 
						else if (stuff[0] == '^' || stuff[0] == '[') {
							stuff = "\\\\" + stuff;
						}
						result_string = result_string + "[" + stuff + "]";
					}
				} 
				else {
					// SPECIAL_CHARS
					// closing ')', '}' and ']'
					// '-' (a range in character set)
					// '&', '~', (extended character set operations)
					// '#' (comment) and WHITESPACE (ignored) in verbose mode
					static std::map<int, std::string> special_characters_map;
					if (special_characters_map.empty()) {
						for (auto &&sc : SPECIAL_CHARACTERS) {
							special_characters_map.emplace(static_cast<int>(sc), std::string{"\\"} + std::string(1, sc));
						}
					}

					if (SPECIAL_CHARACTERS.find(c) != std::string_view::npos) {
						result_string += special_characters_map[static_cast<int>(c)];
					} 
					else {
						result_string += c;
					}
				}
			}
			return std::string{"(("} + result_string + std::string{R"()|[\r\n])$)"};
		}

		std::regex compile_pattern(std::string_view pattern) {
			return std::regex(translate(pattern), std::regex::ECMAScript);
		}

		bool fnmatch(std::string&& name, const std::regex& pattern) {
			return std::regex_match(std::move(name), pattern);
		}

		std::vector<fs::path> filter(const std::vector<fs::path> &names,
																 std::string_view pattern) {
			// std::cout << "Pattern: " << pattern << "\n";
			const auto pattern_re = compile_pattern(pattern);
			std::vector<fs::path> result;
			std::copy_if(std::make_move_iterator(names.begin()), std::make_move_iterator(names.end()),
									 std::back_inserter(result),
									 [&pattern_re](const fs::path& name) {
												 // std::cout << "Checking for " << name.string() << "\n";
												 return fnmatch(name.string(), pattern_re);
						 });
			return result;
		}

		fs::path expand_tilde(fs::path path) {
			if (path.empty()) return path;

			auto firstdirname = *(path.begin());

			if (path.is_relative() && firstdirname == "~") {
				// expand tilde, when it's at the start of the (relative) path.
#ifdef _WIN32
				char* home;
				size_t sz;
				errno_t err = _dupenv_s(&home, &sz, "USERPROFILE");
				if (home == nullptr) {
					err = _dupenv_s(&home, &sz, "HOME");
					if (home == nullptr) {
						throw std::invalid_argument("error: Unable to expand `~` - neither USERPROFILE nor HOME environment variables are set.");
					}
				}
#else
				const char* home = std::getenv("HOME");
				if (home == nullptr) {
					throw std::invalid_argument("error: Unable to expand `~` - HOME environment variable not set.");
				}
#endif

				std::string s = path.string();
				s = std::string{home} + s.substr(1, s.size() - 1);
#ifdef _WIN32
				free(home);
#endif
				return fs::path(s).lexically_normal();
			}
			return path;
		}

		bool has_magic(const std::string &pathname) {
			static const auto magic_check = std::regex("([*?[])");
			return std::regex_search(pathname, magic_check);
		}

		bool has_magic(const fs::path &path) {
			auto pathname = path.string();
			return has_magic(pathname);
		}

		constexpr bool is_hidden(std::string_view pathname) noexcept {
			return pathname[0] == '.';
		}

		bool is_hidden(const fs::path &path) noexcept {
			std::string pathname = path.filename().string();
			return pathname[0] == '.';
		}

		constexpr bool is_recursive(std::string_view pattern) noexcept {
			return pattern == std::string_view{"**"};
		}

		constexpr bool is_recursive(const std::string &pattern) noexcept {
			return pattern == "**";
		}

		bool is_recursive(const fs::path &path_element) noexcept {
			std::string pattern = path_element.string();
			return pattern == "**";
		}

		//
		// hotfix because on MS Windows some path' symlinks throw a spanner in the woodworks by being expanded, e.g.
		//
		//    fs::relative("C:\\Users\\All Users", "C:\\Users") != "All Users"
		// 
		// but returns "..\\ProgramData" instead, and we don't want that, hence we get rid of the implicit weak_canonical() calls
		// applied to either path:
		//
		fs::path mk_relative(const fs::path& p, const fs::path& base) {
			//return weakly_canonical(p).lexically_relative(weakly_canonical(base));
			return p.lexically_relative(base);
		}

		std::vector<fs::path> iter_directory(const fs::path &dirname, bool dironly) {
			std::vector<fs::path> result;

			auto current_directory = dirname;
			if (current_directory.empty()) {
				current_directory = fs::current_path();
			}

			if (fs::exists(current_directory)) {
				try {
					for (auto &entry : fs::directory_iterator(
						current_directory, fs::directory_options::follow_directory_symlink |
						fs::directory_options::skip_permission_denied)) {
						if (!dironly || entry.is_directory()) {
							if (dirname.is_absolute()) {
								result.push_back(entry.path());
							} 
							else {
								result.push_back(fs::relative(entry.path()));
							}
						}
					}
				}
				catch (std::exception&) {
					// not a directory
					// do nothing
				}
			}

			return result;
		}

		// Recursively yields relative pathnames inside a literal directory.
		std::vector<fs::path> rlistdir(const fs::path &dirname, bool dironly) {
			std::vector<fs::path> result;
			//std::cout << "rlistdir: " << dirname.string() << "\n";
			auto names = iter_directory(dirname, dironly);
			for (auto &&name : names) {
				if (!is_hidden(name)) {
					result.push_back(name);
					auto matched_dirs = rlistdir(name, dironly);
					std::copy(std::make_move_iterator(matched_dirs.begin()), std::make_move_iterator(matched_dirs.end()), std::back_inserter(result));
				}
			}
			return result;
		}

		// This helper function recursively yields relative pathnames inside a literal
		// directory.
		std::vector<fs::path> glob2(const fs::path &dirname, [[maybe_unused]] const fs::path &pattern,
																bool dironly) {
			// std::cout << "In glob2\n";
			std::vector<fs::path> result{"."};
			assert(is_recursive(pattern));
			auto matched_dirs = rlistdir(dirname, dironly);
			std::copy(std::make_move_iterator(matched_dirs.begin()), std::make_move_iterator(matched_dirs.end()), std::back_inserter(result));
			return result;
		}

		// These 2 helper functions non-recursively glob inside a literal directory.
		// They return a list of basenames.  _glob1 accepts a pattern while _glob0
		// takes a literal basename (so it only has to check for its existence).

		std::vector<fs::path> glob1(const fs::path &dirname, const fs::path &pattern,
																bool dironly) {
			// std::cout << "In glob1\n";
			std::vector<fs::path> filtered_names;
			auto names = iter_directory(dirname, dironly);
			for (auto &&name : names) {
				if (!is_hidden(name)) {
					filtered_names.push_back(name.filename());
					// if (name.is_relative()) {
					//   // std::cout << "Filtered (Relative): " << name << "\n";
					//   filtered_names.push_back(fs::relative(name));
					// } else {
					//   // std::cout << "Filtered (Absolute): " << name << "\n";
					//   filtered_names.push_back(name.filename());
					// }
				}
			}
			return filter(filtered_names, pattern.string());
		}

		std::vector<fs::path> glob0(const fs::path &dirname, const fs::path &basename,
																bool /*dironly*/) {
			// std::cout << "In glob0\n";

			// 'q*x/' should match only directories.
			if ((basename.empty() && fs::is_directory(dirname)) || (!basename.empty() && fs::exists(dirname / basename))) {
				return {basename};
			}
			return {};
		}

		std::vector<fs::path> glob(const fs::path &pathspec, bool recursive = false,
															 bool dironly = false) {
			std::vector<fs::path> result;

			fs::path path = pathspec;

			path = expand_tilde(path);

			auto dirname = path.parent_path();
			const auto basename = path.filename().string();
			auto pathname = path.string();

			if (!has_magic(pathname)) {
				assert(!dironly);

				// Patterns ending with a slash should match only directories
				if ((!basename.empty() && fs::exists(path)) || (basename.empty() && fs::is_directory(dirname))) {
					result.push_back(path);
				}
				return result;
			}

			if (dirname.empty()) {
				if (recursive && is_recursive(basename)) {
					return glob2(dirname, basename, dironly);
				}
				return glob1(dirname, basename, dironly);
			}

			std::vector<fs::path> dirs{dirname};
			if (dirname != fs::path(pathname) && has_magic(dirname.string())) {
				dirs = glob(dirname, recursive, true);
			}

			auto glob_in_dir = glob0;
			if (has_magic(basename)) {
				if (recursive && is_recursive(basename)) {
					glob_in_dir = glob2;
				} 
				else {
					glob_in_dir = glob1;
				}
			}

			for (auto &d : dirs) {
				for (auto &&name : glob_in_dir(d, basename, dironly)) {
					fs::path subresult = name;
					if (name.parent_path().empty()) {
						subresult = d / name;
					}
					result.push_back(subresult.lexically_normal());
				}
			}

			return result;
		}

		std::vector<fs::path> glob(const std::string& pathname, bool recursive = false,
			bool dironly = false) {
			return glob(fs::path(pathname), recursive, dironly);
		}

		struct searchspec {
			fs::path basepath;		// the non-wildcarded base
			fs::path deep_spec;   // the rest of the searchspec; may contain wildcards

			bool accepts_directories_only;		// when the search spec ended in a '/', that signaled the user only wishes to receive directory entries.

			int actual_depth;
			int max_recursion_depth;  // -1 means: unlimited depth

			int original_spec_index;
		};

		struct cached_options {
			fs::path basepath;
			std::vector<searchspec> searchpaths;
			int searchpath_index = -1;

			int item_count_scanned = 0;
			int dir_count_scanned = 0;

			std::vector<fs::path> result_set;
			std::vector<std::string> error_msg;
		};

		bool glob_42(cached_options &cache, options &search_spec) {
			// preparation / init phase
			if (cache.searchpath_index < 0) {
				cache.basepath = search_spec.basepath;
				if (!cache.basepath.empty())
					cache.basepath = expand_tilde(cache.basepath);

				for (int index = 0; index < search_spec.pathnames.size(); index++) {
					fs::path pn = search_spec.pathnames[index];
					pn = expand_tilde(pn);
					bool is_rel = pn.is_relative();

					if (pn.empty()) {
						pn = fs::current_path();
						is_rel = false;
					}

					int max_depth = search_spec.max_recursion_depth[index];
					if (max_depth < 0)
						max_depth = INT_MAX;

					// help detect whether the search spec ended with an '/' or equivalent directory separator:
					const auto basename = pn.filename().string();

					searchspec spec{
						.basepath = (is_rel ? cache.basepath : ""),
						.deep_spec = pn,
						.accepts_directories_only = basename.empty(),
						.actual_depth = 0,
						.max_recursion_depth = max_depth,
						.original_spec_index = index,
					};
					cache.searchpaths.push_back(spec);
				}

				cache.item_count_scanned = 0;
				cache.dir_count_scanned = 0;

				cache.searchpath_index = 0;
			}

			if (cache.searchpath_index >= cache.searchpaths.size())
				return false;

			searchspec pathspec = cache.searchpaths[cache.searchpath_index];
			if (pathspec.actual_depth > pathspec.max_recursion_depth)
				return true;

			try {
				assert(!pathspec.deep_spec.empty());

				if (!has_magic(pathspec.deep_spec)) {
					assert(!has_magic(pathspec.basepath));

					fs::path path = pathspec.basepath / pathspec.deep_spec;

					bool is_dir = fs::is_directory(path);

					if (is_dir)
						cache.dir_count_scanned++;
					else
						cache.item_count_scanned++;

					options::filter_info_t fi{
						.basepath = pathspec.basepath,
						.filename = pathspec.deep_spec,

						.matching_wildcarded_fragment = pathspec.deep_spec,
						.subsearch_spec = "",

						.fragment_is_wildcarded = false,
						.fragment_is_double_star = false,

						.userland_may_override_accept = true,
						.userland_may_override_recurse_into = is_dir,

						.is_directory = is_dir,
						.is_hidden = is_hidden(path),

						.depth = pathspec.actual_depth,
						.max_recursion_depth = pathspec.max_recursion_depth,

						.search_spec_index = pathspec.original_spec_index
					};
					// Note: patterns ending with a slash should match only directories.
					options::filter_state_t fs{
						.accept = (search_spec.include_hidden_entries || !fi.is_hidden) &&
											(is_dir ? search_spec.include_matching_directories : search_spec.include_matching_files && !pathspec.accepts_directories_only && fs::exists(path)),
						.recurse_into = false,
						.stop_scan_for_this_spec = false,
						.do_report_progress = false,
					};
					fs = search_spec.filter(path, fs, fi);

					if (fs.accept) {
						cache.result_set.push_back(path);
					}

					// Note: we do accept a 'recurse_info' override by userland filter here anyway, while the original search spec didn't mandate/suppose that sort of thing.
					// Userland overrides work both ways...
					if (fs.recurse_into && is_dir && pathspec.actual_depth < pathspec.max_recursion_depth) {
						searchspec spec{
							.basepath = path,
							.deep_spec = "*",
							.accepts_directories_only = false,   // we drop this requirement here as the userland override has us going out of original scope already.
							.actual_depth = pathspec.actual_depth + 1,
							.max_recursion_depth = pathspec.max_recursion_depth,
							.original_spec_index = pathspec.original_spec_index,
						};
						cache.searchpaths.push_back(spec);
					}

					if (fs.do_report_progress) {
						options::progress_info_t pi{
							.current_path = path,
							.path_filter_info = fi,
							.path_filter_state = fs,
							.item_count_scanned = cache.item_count_scanned,
							.dir_count_scanned = cache.dir_count_scanned,
							.dir_count_todo = 0,
							.searchpath_queue_index = cache.searchpath_index,
							.searchpath_queue_size = (int)cache.searchpaths.size(),
						};
						if (!search_spec.progress_reporting(pi))
							return false;
					}

					if (fs.stop_scan_for_this_spec) {
						return true;
					}
				} 
				else {
					assert(!pathspec.deep_spec.empty());
					assert(has_magic(pathspec.deep_spec));

					fs::path basepath = pathspec.basepath;

					for (auto it = pathspec.deep_spec.begin(); /* it != pathspec.deep_spec.end() */; ++it) {
						fs::path elem = *it;
						if (!has_magic(elem)) {
							basepath /= elem;
							continue;
						}

						bool recursive_scan_dirtree = is_recursive(elem);

						fs::path sub_spec;
						for (it++; it != pathspec.deep_spec.end(); ++it) {
							sub_spec /= *it;
						}

						// are we processing a '**' wildcard? If we do, we MAY also match empty/NIL, i.e. '**' matching exactly *nothing*:
						// that's what we deal with right now.
						if (recursive_scan_dirtree) {
							bool is_dir = fs::is_directory(basepath);

							if (is_dir)
								cache.dir_count_scanned++;
							else
								cache.item_count_scanned++;

							options::filter_info_t fi{
								.basepath = basepath,
								.filename = "",

								.matching_wildcarded_fragment = elem,
								.subsearch_spec = sub_spec,

								.fragment_is_wildcarded = true,
								.fragment_is_double_star = true,

								.userland_may_override_accept = true,
								.userland_may_override_recurse_into = is_dir,

								.is_directory = is_dir,
								.is_hidden = is_hidden(basepath),

								.depth = pathspec.actual_depth,
								.max_recursion_depth = pathspec.max_recursion_depth,

								.search_spec_index = pathspec.original_spec_index
							};
							// Note: patterns ending with a slash should match only directories.
							options::filter_state_t fs{
								.accept = (search_spec.include_hidden_entries || !fi.is_hidden) &&
													sub_spec.empty() &&
													(is_dir ? search_spec.include_matching_directories : search_spec.include_matching_files && !pathspec.accepts_directories_only && fs::exists(basepath)),
								.recurse_into = is_dir,
								.stop_scan_for_this_spec = false,
								.do_report_progress = false,
							};
							fs = search_spec.filter(basepath, fs, fi);

							if (fs.accept) {
								cache.result_set.push_back(basepath);
							}

							if (fs.recurse_into && is_dir) {
								if (!sub_spec.empty()) {
									// this effectively drops the '**' from the search path...
									searchspec spec{
										.basepath = basepath,
										.deep_spec = sub_spec,
										.accepts_directories_only = pathspec.accepts_directories_only,
										.actual_depth = pathspec.actual_depth,
										.max_recursion_depth = pathspec.max_recursion_depth,
										.original_spec_index = pathspec.original_spec_index,
									};
									cache.searchpaths.push_back(spec);
								} 
								else {
									// when there's no further (possibly wildcarded) search spec following the '**', then we assume it is '/*', i.e.
									//    /bla/**
									// is assumed identical to
									//    /bla/**/*
									searchspec spec{
										.basepath = basepath,
										.deep_spec = (sub_spec.empty() ? "*" : sub_spec),
										.accepts_directories_only = pathspec.accepts_directories_only,
										.actual_depth = pathspec.actual_depth,
										.max_recursion_depth = pathspec.max_recursion_depth,
										.original_spec_index = pathspec.original_spec_index,
									};
									cache.searchpaths.push_back(spec);
								}
							}

							if (fs.do_report_progress) {
								options::progress_info_t pi{
									.current_path = basepath,
									.path_filter_info = fi,
									.path_filter_state = fs,
									.item_count_scanned = cache.item_count_scanned,
									.dir_count_scanned = cache.dir_count_scanned,
									.dir_count_todo = 0,
									.searchpath_queue_index = cache.searchpath_index,
									.searchpath_queue_size = (int)cache.searchpaths.size(),
								};
								if (!search_spec.progress_reporting(pi))
									return false;
							}

							if (fs.stop_scan_for_this_spec) {
								return true;
							}

							// now process the "**" element further: scan the current directory for any subdirectories and recurse into them.
							// Do this recursively as "**" can match multiple levels of path hierarchy.
							std::vector<fs::path> dirs_to_scan;
							std::vector<int> dir_depths;
							dirs_to_scan.push_back(basepath);
							dir_depths.push_back(pathspec.actual_depth);

							for (int index = 0; index < dirs_to_scan.size(); index++) {
								fs::path dirpath = dirs_to_scan[index];
								int dirdepth = dir_depths[index];

								if (dirdepth > pathspec.max_recursion_depth)
									continue;

								for (auto &&entry : fs::directory_iterator(dirpath, fs::directory_options::follow_directory_symlink | fs::directory_options::skip_permission_denied)) {
									fs::path path = entry.path();

									bool is_dir = fs::is_directory(path);

									if (is_dir)
										cache.dir_count_scanned++;
									else
										cache.item_count_scanned++;

									if (!is_dir)
										continue;

									if (cache.item_count_scanned > 10000)
										break;

									options::filter_info_t fi{
										.basepath = dirpath,
										.filename = mk_relative(path, dirpath),

										.matching_wildcarded_fragment = elem,
										.subsearch_spec = sub_spec,

										.fragment_is_wildcarded = true,
										.fragment_is_double_star = true,

										.userland_may_override_accept = false,
										.userland_may_override_recurse_into = true,

										.is_directory = true,
										.is_hidden = is_hidden(dirpath),

										.depth = dirdepth + 1,
										.max_recursion_depth = pathspec.max_recursion_depth,

										.search_spec_index = pathspec.original_spec_index
									};
									options::filter_state_t fs{
										.accept = (search_spec.include_hidden_entries || !fi.is_hidden) &&
															sub_spec.empty() &&
															search_spec.include_matching_directories,
										.recurse_into = true,
										.stop_scan_for_this_spec = false,
										.do_report_progress = false,
									};
									fs = search_spec.filter(path, fs, fi);

									if (fs.accept) {
										cache.result_set.push_back(path);
									}

									// when there's no further (possibly wildcarded) search spec following the '**', then we assume it is '/*', i.e.
									//    /bla/**
									// is assumed identical to
									//    /bla/**/*
									if (fs.recurse_into && is_dir && dirdepth < pathspec.max_recursion_depth) {
										searchspec spec{
											.basepath = path,
											.deep_spec = (sub_spec.empty() ? "*" : sub_spec),
											.accepts_directories_only = pathspec.accepts_directories_only,
											.actual_depth = dirdepth + 1,
											.max_recursion_depth = pathspec.max_recursion_depth,
											.original_spec_index = pathspec.original_spec_index,
										};
										cache.searchpaths.push_back(spec);

										dirs_to_scan.push_back(path);
										dir_depths.push_back(dirdepth + 1);
									}

									if (fs.do_report_progress) {
										options::progress_info_t pi{
											.current_path = path,
											.path_filter_info = fi,
											.path_filter_state = fs,
											.item_count_scanned = cache.item_count_scanned,
											.dir_count_scanned = cache.dir_count_scanned,
											.dir_count_todo = 0,
											.searchpath_queue_index = cache.searchpath_index,
											.searchpath_queue_size = (int)cache.searchpaths.size(),
										};
										if (!search_spec.progress_reporting(pi))
											return false;
									}

									if (fs.stop_scan_for_this_spec) {
										return true;
									}
								}
							}
						} 
						else {
							assert(!recursive_scan_dirtree);

							// we are NOT processing a '**' wildcard, but a (wildcarded) subspec instead, e.g. "*bla*/reutel.pdf" or "*ska*.mp3"...
							if (!sub_spec.empty())
							{
								// scan wildcarded directory spec element, e.g. "*bla*/" in "*bla*/reutel.pdf", hence we will only accept matching directory names here.
								for (auto &&entry : fs::directory_iterator(basepath, fs::directory_options::follow_directory_symlink | fs::directory_options::skip_permission_denied)) {
									fs::path path = entry.path();

									bool is_dir = fs::is_directory(path);

									if (is_dir)
										cache.dir_count_scanned++;
									else
										cache.item_count_scanned++;

									if (!is_dir)
										continue;

									if (cache.item_count_scanned > 10000)
										break;

									options::filter_info_t fi{
										.basepath = basepath,
										.filename = mk_relative(path, basepath),

										.matching_wildcarded_fragment = elem,
										.subsearch_spec = sub_spec,

										.fragment_is_wildcarded = true,
										.fragment_is_double_star = false,

										.userland_may_override_accept = true,
										.userland_may_override_recurse_into = true,

										.is_directory = true,
										.is_hidden = is_hidden(path),

										.depth = pathspec.actual_depth + 1,
										.max_recursion_depth = pathspec.max_recursion_depth,

										.search_spec_index = pathspec.original_spec_index
									};
									options::filter_state_t fs{
										.accept = (search_spec.include_hidden_entries || !fi.is_hidden) &&
															search_spec.include_matching_directories,
										.recurse_into = true,
										.stop_scan_for_this_spec = false,
										.do_report_progress = false,
									};
									fs = search_spec.filter(path, fs, fi);

									if (fs.accept) {
										cache.result_set.push_back(path);
									}

									if (fs.recurse_into && is_dir && pathspec.actual_depth < pathspec.max_recursion_depth) {
										searchspec spec{
											.basepath = path,
											.deep_spec = sub_spec,
											.accepts_directories_only = pathspec.accepts_directories_only,
											.actual_depth = pathspec.actual_depth + 1,
											.max_recursion_depth = pathspec.max_recursion_depth,
											.original_spec_index = pathspec.original_spec_index,
										};
										cache.searchpaths.push_back(spec);
									}

									if (fs.do_report_progress) {
										options::progress_info_t pi{
											.current_path = path,
											.path_filter_info = fi,
											.path_filter_state = fs,
											.item_count_scanned = cache.item_count_scanned,
											.dir_count_scanned = cache.dir_count_scanned,
											.dir_count_todo = 0,
											.searchpath_queue_index = cache.searchpath_index,
											.searchpath_queue_size = (int)cache.searchpaths.size(),
										};
										if (!search_spec.progress_reporting(pi))
											return false;
									}

									if (fs.stop_scan_for_this_spec) {
										return true;
									}
								}
							} 
							else {
								// scan wildcarded filename spec element, e.g. "*ska*.mp3", hence we will accept both matching files and matching directory names here.
								assert(sub_spec.empty());

								for (auto &&entry : fs::directory_iterator(basepath, fs::directory_options::follow_directory_symlink | fs::directory_options::skip_permission_denied)) {
									fs::path path = entry.path();

									bool is_dir = fs::is_directory(path);

									if (is_dir)
										cache.dir_count_scanned++;
									else
										cache.item_count_scanned++;

									if (cache.item_count_scanned > 10000)
										break;

									options::filter_info_t fi{
										.basepath = basepath,
										.filename = mk_relative(path, basepath),

										.matching_wildcarded_fragment = elem,
										.subsearch_spec = "",

										.fragment_is_wildcarded = true,
										.fragment_is_double_star = false,

										.userland_may_override_accept = true,
										.userland_may_override_recurse_into = is_dir,

										.is_directory = is_dir,
										.is_hidden = is_hidden(path),

										.depth = pathspec.actual_depth,
										.max_recursion_depth = pathspec.max_recursion_depth,

										.search_spec_index = pathspec.original_spec_index
									};
									options::filter_state_t fs{
										.accept = (search_spec.include_hidden_entries || !fi.is_hidden) &&
															(is_dir ? search_spec.include_matching_directories : search_spec.include_matching_files /* && fs::exists(path) */),
										.recurse_into = is_dir,
										.stop_scan_for_this_spec = false,
										.do_report_progress = false,
									};
									fs = search_spec.filter(path, fs, fi);

									if (fs.accept) {
										cache.result_set.push_back(path);
									}

									// Note: we do accept a 'recurse_info' override by userland filter here anyway, while the original search spec didn't mandate/suppose that sort of thing.
									// Userland overrides work both ways...
									if (fs.recurse_into && is_dir && pathspec.actual_depth < pathspec.max_recursion_depth) {
										searchspec spec{
											.basepath = path,
											.deep_spec = "*",
											.accepts_directories_only = false,   // we drop this requirement here as the userland override has us going out of original scope already.
											.actual_depth = pathspec.actual_depth + 1,
											.max_recursion_depth = pathspec.max_recursion_depth,
											.original_spec_index = pathspec.original_spec_index,
										};
										cache.searchpaths.push_back(spec);
									}

									if (fs.do_report_progress) {
										options::progress_info_t pi{
											.current_path = path,
											.path_filter_info = fi,
											.path_filter_state = fs,
											.item_count_scanned = cache.item_count_scanned,
											.dir_count_scanned = cache.dir_count_scanned,
											.dir_count_todo = 0,
											.searchpath_queue_index = cache.searchpath_index,
											.searchpath_queue_size = (int)cache.searchpaths.size(),
										};
										if (!search_spec.progress_reporting(pi))
											return false;
									}

									if (fs.stop_scan_for_this_spec) {
										return true;
									}
								}
							}
						}

						// all done.
						break;
					}
				}
			}
			catch (std::exception& ex) {
				// not a directory
				// do nothing
				fs::path searchpath = pathspec.deep_spec / pathspec.deep_spec;
				auto msg = std::format("{}: {}\n", searchpath.string(), ex.what());
				cache.error_msg.push_back(msg);
			}

			return true;
		}

	} // namespace end


	/// \param pathname string containing a path specification
	/// \return vector of paths that match the pathname
	///
	/// Pathnames can be absolute (/usr/src/Foo/Makefile) or relative (../../Tools/*/*.gif)
	/// Pathnames can contain shell-style wildcards
	/// Broken symlinks are included in the results (as in the shell)
	std::vector<fs::path> glob(const std::string &pathname) {
		return glob(pathname, false);
	}

	/// \param basepath the root directory to run in
	/// \param pathname string containing a path specification
	/// \return vector of paths that match the pathname
	///
	/// Pathnames can be absolute (/usr/src/Foo/Makefile) or relative (../../Tools/*/*.gif)
	/// Pathnames can contain shell-style wildcards
	/// Broken symlinks are included in the results (as in the shell)
	std::vector<fs::path> glob_path(const std::string& basepath, const std::string& pathname) {
		return glob(fs::path(basepath) / pathname, false);
	}

	/// \param pathnames string containing a path specification
	/// \return vector of paths that match the pathname
	///
	/// Globs recursively.
	/// The pattern “**” will match any files and zero or more directories, subdirectories and
	/// symbolic links to directories.
	std::vector<fs::path> rglob(const std::string &pathname) {
		return glob(pathname, true);
	}

	/// \param basepath the root directory to run in
	/// \param pathnames string containing a path specification
	/// \return vector of paths that match the pathname
	///
	/// Globs recursively.
	/// The pattern “**” will match any files and zero or more directories, subdirectories and
	/// symbolic links to directories.
	std::vector<fs::path> rglob_path(const std::string& basepath, const std::string& pathname) {
		return glob(fs::path(basepath) / pathname, true);
	}


	/// Runs `glob` against each pathname in `pathnames` and accumulates the results
	std::vector<fs::path> glob(const std::vector<std::string> &pathnames) {
		std::vector<fs::path> result;
		for (const auto &pathname : pathnames) {
			auto matched_res = glob(pathname, false);
			std::copy(std::make_move_iterator(matched_res.begin()), std::make_move_iterator(matched_res.end()), std::back_inserter(result));
		}
		return result;
	}

	/// Runs `glob` against each pathname in `pathnames` and accumulates the results
	std::vector<fs::path> glob_path(const std::string& basepath, const std::vector<std::string>& pathnames) {
		std::vector<fs::path> result;
		for (auto& pathname : pathnames)
		{
			for (auto& match : glob(fs::path(basepath) / pathname, false))
			{
				result.push_back(std::move(match));
			}
		}
		return result;
	}

	/// Runs `rglob` against each pathname in `pathnames` and accumulates the results
	std::vector<fs::path> rglob(const std::vector<std::string> &pathnames) {
		std::vector<fs::path> result;
		for (const auto &pathname : pathnames) {
			auto matched_res = glob(pathname, true);
			std::copy(std::make_move_iterator(matched_res.begin()), std::make_move_iterator(matched_res.end()), std::back_inserter(result));
		}
		return result;
	}

	/// Runs `rglob` against each pathname in `pathnames` and accumulates the results
	std::vector<fs::path> rglob_path(const std::string& basepath, const std::vector<std::string>& pathnames) {
		std::vector<fs::path> result;
		for (auto &pathname : pathnames) {
			for (auto &match : glob(fs::path(basepath) / pathname, true)) {
				result.push_back(std::move(match));
			}
		}
		return result;
	}


	/// Initializer list overload for convenience
	std::vector<fs::path> glob(const std::initializer_list<std::string> &pathnames) {
		return glob(std::vector<std::string>(pathnames));
	}


	/// Initializer list overload for convenience
	std::vector<fs::path> glob_path(const std::string& basepath, const std::initializer_list<std::string>& pathnames) {
		return glob_path(basepath, std::vector<std::string>(pathnames));
	}


	/// Initializer list overload for convenience
	std::vector<fs::path> rglob(const std::initializer_list<std::string> &pathnames) {
		return rglob(std::vector<std::string>(pathnames));
	}


	/// Initializer list overload for convenience
	std::vector<fs::path> rglob_path(const std::string& basepath, const std::initializer_list<std::string>& pathnames) {
		return rglob_path(basepath, std::vector<std::string>(pathnames));
	}


	std::vector<fs::path> glob(const options &search_spec) {
		options spec(search_spec);
		return glob(spec);
	}

	std::vector<fs::path> glob(options &search_spec) {
		cached_options cache;

		while (glob_42(cache, search_spec)) {
			cache.searchpath_index++;
		}

		return cache.result_set;
	}


	// filter callback: returns pass/reject for given path; this can override the default glob reject/accept logic in either direction
	// as both rejected and accepted entries are fed to this callback method.
	options::filter_state_t options::filter(fs::path path, const options::filter_state_t glob_says_pass, const options::filter_info_t &info) {
		return glob_says_pass;
	}

	// progress callback: shows currently processed path, pass/reject status and progress/scan completion estimate.
	// Return `false` to abort the glob action.
	bool options::progress_reporting(const progress_info_t &info) {
		return true;
	}


	/// Helper function: expand '~' HOME part (when used in the path) and normalize the given path.
	fs::path expand_and_normalize_tilde(fs::path path) {
		path = expand_tilde(path);
		return path.lexically_normal();
	}

} // namespace glob
