
#pragma once
#include <filesystem>
#include <string>
#include <vector>

#ifdef GLOBLIB_SEPARATE_COMPILATION
#define GLOBLIB_IMPL
#else
#define GLOBLIB_IMPL static inline
#endif

namespace fs = std::filesystem;

namespace glob {

/// Helper struct for extended options
struct options {
  fs::path basepath;
  std::vector<fs::path> pathnames;
  bool include_hidden_entries = false;

  /// \param basepath the root directory to run in
  /// \param pathname string containing a path specification
  /// Convenience constructor for use when only a single pathspec will be used
  options(const fs::path& basepath, const fs::path& pathname, bool include_hidden_entries = false) : basepath(basepath), pathnames({pathname}), include_hidden_entries(include_hidden_entries) {};
};

/// \param pathname string containing a path specification
/// \return vector of paths that match the pathname
///
/// Pathnames can be absolute (/usr/src/Foo/Makefile) or relative (../../Tools/*/*.gif)
/// Pathnames can contain shell-style wildcards
/// Broken symlinks are included in the results (as in the shell)
GLOBLIB_IMPL
std::vector<fs::path> glob(const std::string &pathname);

/// \param basepath the root directory to run in
/// \param pathname string containing a path specification
/// \return vector of paths that match the pathname
///
/// Pathnames can be absolute (/usr/src/Foo/Makefile) or relative (../../Tools/*/*.gif)
/// Pathnames can contain shell-style wildcards
/// Broken symlinks are included in the results (as in the shell)
GLOBLIB_IMPL
std::vector<std::filesystem::path> glob_path(const std::string& basepath, const std::string& pathname);

/// \param pathnames string containing a path specification
/// \return vector of paths that match the pathname
///
/// Globs recursively.
/// The pattern �**� will match any files and zero or more directories, subdirectories and
/// symbolic links to directories.
GLOBLIB_IMPL
std::vector<fs::path> rglob(const std::string &pathname);

/// \param basepath the root directory to run in
/// \param pathnames string containing a path specification
/// \return vector of paths that match the pathname
///
/// Globs recursively.
/// The pattern �**� will match any files and zero or more directories, subdirectories and
/// symbolic links to directories.
GLOBLIB_IMPL
std::vector<std::filesystem::path> rglob_path(const std::string& basepath, const std::string& pathname);

/// Runs `glob` against each pathname in `pathnames` and accumulates the results
GLOBLIB_IMPL
std::vector<std::filesystem::path> glob(const std::vector<std::string> &pathnames);

/// Runs `glob` against each pathname in `pathnames` and accumulates the results
GLOBLIB_IMPL
std::vector<std::filesystem::path> glob_path(const std::string& basepath, const std::vector<std::string>& pathnames);

/// Runs `rglob` against each pathname in `pathnames` and accumulates the results
GLOBLIB_IMPL
std::vector<std::filesystem::path> rglob(const std::vector<std::string> &pathnames);

/// Runs `rglob` against each pathname in `pathnames` and accumulates the results
GLOBLIB_IMPL
std::vector<std::filesystem::path> rglob_path(const std::string& basepath, const std::vector<std::string>& pathnames);

/// Initializer list overload for convenience
GLOBLIB_IMPL
std::vector<std::filesystem::path>
glob(const std::initializer_list<std::string> &pathnames);

/// Initializer list overload for convenience
GLOBLIB_IMPL
std::vector<std::filesystem::path> glob_path(const std::string& basepath,
											 const std::initializer_list<std::string>& pathnames);
/// Initializer list overload for convenience
GLOBLIB_IMPL
std::vector<std::filesystem::path>
rglob(const std::initializer_list<std::string> &pathnames);

/// Initializer list overload for convenience
GLOBLIB_IMPL
std::vector<std::filesystem::path> rglob_path(const std::string& basepath,
											  const std::initializer_list<std::string>& pathnames);
} // namespace glob

#if (!defined(GLOBLIB_SEPARATE_COMPILATION) || (defined(GLOBLIB_SEPARATE_COMPILATION) && defined(GLOBLIB_INCLUDE_IMPLEMENTATION)))

#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <regex>

namespace glob
{
	namespace
	{
		bool string_replace(std::string& str, const std::string& from, const std::string& to)
		{
			std::size_t start_pos = str.find(from);
			if (start_pos == std::string::npos)
				return false;
			str.replace(start_pos, from.length(), to);
			return true;
		}

		std::string translate(const std::string& pattern)
		{
			std::size_t i = 0, n = pattern.size();
			std::string result_string;

			while (i < n)
			{
				auto c = pattern[i];
				i += 1;
				if (c == '*')
				{
					result_string += ".*";
				}
				else if (c == '?')
				{
					result_string += ".";
				}
				else if (c == '[')
				{
					auto j = i;
					if (j < n && pattern[j] == '!')
					{
						j += 1;
					}
					if (j < n && pattern[j] == ']')
					{
						j += 1;
					}
					while (j < n && pattern[j] != ']')
					{
						j += 1;
					}
					if (j >= n)
					{
						result_string += "\\[";
					}
					else
					{
						auto stuff = std::string(pattern.begin() + i, pattern.begin() + j);
						if (stuff.find("--") == std::string::npos)
						{
							string_replace(stuff, std::string {"\\"}, std::string {R"(\\)"});
						}
						else
						{
							std::vector<std::string> chunks;
							std::size_t k = 0;
							if (pattern[i] == '!')
							{
								k = i + 2;
							}
							else
							{
								k = i + 1;
							}

							while (true)
							{
								k = pattern.find("-", k, j);
								if (k == std::string::npos)
								{
									break;
								}
								chunks.push_back(std::string(pattern.begin() + i, pattern.begin() + k));
								i = k + 1;
								k = k + 3;
							}

							chunks.push_back(std::string(pattern.begin() + i, pattern.begin() + j));
							// Escape backslashes and hyphens for set difference (--).
							// Hyphens that create ranges shouldn't be escaped.
							bool first = false;
							for (auto& s : chunks)
							{
								string_replace(s, std::string {"\\"}, std::string {R"(\\)"});
								string_replace(s, std::string {"-"}, std::string {R"(\-)"});
								if (first)
								{
									stuff += s;
									first = false;
								}
								else
								{
									stuff += "-" + s;
								}
							}
						}

						// Escape set operations (&&, ~~ and ||).
						std::string result;
						std::regex_replace(std::back_inserter(result), // ressult
										   stuff.begin(), stuff.end(), // string
										   std::regex(std::string {R"([&~|])"}), // pattern
										   std::string {R"(\\\1)"}); // repl
						stuff = result;
						i = j + 1;
						if (stuff[0] == '!')
						{
							stuff = "^" + std::string(stuff.begin() + 1, stuff.end());
						}
						else if (stuff[0] == '^' || stuff[0] == '[')
						{
							stuff = "\\\\" + stuff;
						}
						result_string = result_string + "[" + stuff + "]";
					}
				}
				else
				{
					// SPECIAL_CHARS
					// closing ')', '}' and ']'
					// '-' (a range in character set)
					// '&', '~', (extended character set operations)
					// '#' (comment) and WHITESPACE (ignored) in verbose mode
					static std::string special_characters = "()[]{}?*+-|^$\\.&~# \t\n\r\v\f";
					static std::map<int, std::string> special_characters_map;
					if (special_characters_map.empty())
					{
						for (auto& c : special_characters)
						{
							special_characters_map.insert(
								std::make_pair(static_cast<int>(c), std::string {"\\"} + std::string(1, c)));
						}
					}

					if (special_characters.find(c) != std::string::npos)
					{
						result_string += special_characters_map[static_cast<int>(c)];
					}
					else
					{
						result_string += c;
					}
				}
			}
			return std::string {"(("} + result_string + std::string {R"()|[\r\n])$)"};
		}

		std::regex compile_pattern(const std::string& pattern)
		{
			return std::regex(translate(pattern), std::regex::ECMAScript);
		}

		bool fnmatch_case(const fs::path& name, const std::string& pattern)
		{
			return std::regex_match(name.string(), compile_pattern(pattern));
		}

		std::vector<fs::path> filter(const std::vector<fs::path>& names, const std::string& pattern)
		{
			// std::cout << "Pattern: " << pattern << "\n";
			std::vector<fs::path> result;
			for (auto& name : names)
			{
				// std::cout << "Checking for " << name.string() << "\n";
				if (fnmatch_case(name, pattern))
				{
					result.push_back(name);
				}
			}
			return result;
		}

		fs::path expand_tilde(fs::path path)
		{
			if (path.empty())
				return path;
	#ifdef _WIN32
			char* home;
			size_t sz;
			errno_t err = _dupenv_s(&home, &sz, "USERPROFILE");
	#else
			const char* home = std::getenv("HOME");
	#endif
			if (home == nullptr)
			{
				throw std::invalid_argument("error: Unable to expand `~` - HOME environment variable not set.");
			}

			std::string s = path.string();
			if (s[0] == '~')
			{
				s = std::string(home) + s.substr(1, s.size() - 1);
				return fs::path(s);
			}
			else
			{
				return path;
			}
		}

		bool has_magic(const std::string& pathname)
		{
			static const auto magic_check = std::regex("([*?[])");
			return std::regex_search(pathname, magic_check);
		}

		static inline bool is_hidden(const std::string& pathname)
		{
			return std::regex_match(pathname, std::regex("^(.*\\/)*\\.[^\\.\\/]+\\/*$"));
		}

		bool is_recursive(const std::string& pattern)
		{
			return pattern == "**";
		}

		std::vector<fs::path> iter_directory(const fs::path& dirname, bool dironly)
		{
			std::vector<fs::path> result;

			auto current_directory = dirname;
			if (current_directory.empty())
			{
				current_directory = fs::current_path();
			}

			if (fs::exists(current_directory))
			{
				try
				{
					for (auto& entry :
						 fs::directory_iterator(current_directory, fs::directory_options::follow_directory_symlink |
																	   fs::directory_options::skip_permission_denied))
					{
						if (!dironly || entry.is_directory())
						{
							if (dirname.is_absolute())
							{
								result.push_back(entry.path());
							}
							else
							{
								result.push_back(fs::relative(entry.path()));
							}
						}
					}
				}
				catch (std::exception&)
				{
					// not a directory
					// do nothing
				}
			}

			return result;
		}

		// Recursively yields relative pathnames inside a literal directory.
		std::vector<fs::path> rlistdir(const fs::path& dirname, bool dironly, bool includehidden)
		{
			std::vector<fs::path> result;
			auto names = iter_directory(dirname, dironly);
			for (auto& x : names)
			{
				if (includehidden || !is_hidden(x.string()))
				{
					result.push_back(x);
					for (auto& y : rlistdir(x, dironly, includehidden))
					{
						result.push_back(y);
					}
				}
			}
			return result;
		}

		// This helper function recursively yields relative pathnames inside a literal
		// directory.
		std::vector<fs::path> glob2(const fs::path& dirname, const fs::path& pattern, bool dironly, bool includehidden)
		{
			// std::cout << "In glob2\n";
			std::vector<fs::path> result;
			assert(is_recursive(pattern.string()));
			for (auto& dir : rlistdir(dirname, dironly, includehidden))
			{
				result.push_back(dir);
			}
			return result;
		}

		// These 2 helper functions non-recursively glob inside a literal directory.
		// They return a list of basenames.  _glob1 accepts a pattern while _glob0
		// takes a literal basename (so it only has to check for its existence).
		std::vector<fs::path> glob1(const fs::path& dirname, const fs::path& pattern, bool dironly, bool includehidden)
		{
			// std::cout << "In glob1\n";
			auto names = iter_directory(dirname, dironly);
			std::vector<fs::path> filtered_names;
			for (auto& n : names)
			{
				if (includehidden || !is_hidden(n.string()))
				{
					filtered_names.push_back(n.filename());
					// if (n.is_relative()) {
					//   // std::cout << "Filtered (Relative): " << n << "\n";
					//   filtered_names.push_back(fs::relative(n));
					// } else {
					//   // std::cout << "Filtered (Absolute): " << n << "\n";
					//   filtered_names.push_back(n.filename());
					// }
				}
			}
			return filter(filtered_names, pattern.string());
		}

		std::vector<fs::path> glob0(const fs::path& dirname, const fs::path& basename, bool /*dironly*/, bool)
		{
			// std::cout << "In glob0\n";
			std::vector<fs::path> result;
			if (basename.empty())
			{
				// 'q*x/' should match only directories.
				if (fs::is_directory(dirname))
				{
					result = {basename};
				}
			}
			else
			{
				if (fs::exists(dirname / basename))
				{
					result = {basename};
				}
			}
			return result;
		}

		std::vector<fs::path> glob(const fs::path& inpath, bool recursive = false, bool dironly = false, bool includehidden = false)
		{
			std::vector<fs::path> result;

			const auto pathname = inpath.string();
			auto path = fs::path(pathname);

			if (pathname[0] == '~')
			{
				// expand tilde
				path = expand_tilde(path);
			}

			auto dirname = path.parent_path();
			const auto basename = path.filename();

			if (!has_magic(pathname))
			{
				assert(!dironly);
				if (!basename.empty())
				{
					if (fs::exists(path))
					{
						result.push_back(path);
					}
				}
				else
				{
					// Patterns ending with a slash should match only directories
					if (fs::is_directory(dirname))
					{
						result.push_back(path);
					}
				}
				return result;
			}

			if (dirname.empty())
			{
				if (recursive && is_recursive(basename.string()))
				{
					return glob2(dirname, basename, dironly, includehidden);
				}
				else
				{
					return glob1(dirname, basename, dironly, includehidden);
				}
			}

			std::vector<fs::path> dirs;
			if (dirname != fs::path(pathname) && has_magic(dirname.string()))
			{
				dirs = glob(dirname, recursive, true, includehidden);
			}
			else
			{
				dirs = {dirname};
			}

			std::function<std::vector<fs::path>(const fs::path&, const fs::path&, bool, bool)> glob_in_dir;
			if (has_magic(basename.string()))
			{
				if (recursive && is_recursive(basename.string()))
				{
					glob_in_dir = glob2;
				}
				else
				{
					glob_in_dir = glob1;
				}
			}
			else
			{
				glob_in_dir = glob0;
			}

			for (auto& d : dirs)
			{
				for (auto& name : glob_in_dir(d, basename, dironly, includehidden))
				{
					fs::path subresult = name;
					if (name.parent_path().empty())
					{
						subresult = d / name;
					}
					result.push_back(subresult);
				}
			}

			return result;
		}

	} // namespace

	std::vector<fs::path> glob(const std::string& pathname)
	{
		return glob(pathname, false);
	}

	std::vector<std::filesystem::path> glob_path(const std::string& basepath, const std::string& pathname)
	{
		return glob(fs::path(basepath) / pathname, false);
	}

	std::vector<fs::path> rglob(const std::string& pathname)
	{
		return glob(pathname, true);
	}

	std::vector<std::filesystem::path> rglob_path(const std::string& basepath, const std::string& pathname)
	{
		return glob(fs::path(basepath) / pathname, true);
	}

	std::vector<std::filesystem::path> glob(const std::vector<std::string>& pathnames)
	{
		std::vector<std::filesystem::path> result;
		for (auto& pathname : pathnames)
		{
			for (auto& match : glob(pathname, false))
			{
				result.push_back(std::move(match));
			}
		}
		return result;
	}

	std::vector<std::filesystem::path> glob_path(const std::string& basepath, const std::vector<std::string>& pathnames)
	{
		std::vector<std::filesystem::path> result;
		for (auto& pathname : pathnames)
		{
			for (auto& match : glob(fs::path(basepath) / pathname, false))
			{
				result.push_back(std::move(match));
			}
		}
		return result;
	}

	std::vector<std::filesystem::path> rglob(const std::vector<std::string>& pathnames)
	{
		std::vector<std::filesystem::path> result;
		for (auto& pathname : pathnames)
		{
			for (auto& match : glob(pathname, true))
			{
				result.push_back(std::move(match));
			}
		}
		return result;
	}
	std::vector<std::filesystem::path> rglob_path(const std::string& basepath,
												  const std::vector<std::string>& pathnames)
	{
		std::vector<std::filesystem::path> result;
		for (auto& pathname : pathnames)
		{
			for (auto& match : glob(fs::path(basepath) / pathname, true))
			{
				result.push_back(std::move(match));
			}
		}
		return result;
	}

	std::vector<std::filesystem::path> glob(const std::initializer_list<std::string>& pathnames)
	{
		return glob(std::vector<std::string>(pathnames));
	}

	std::vector<std::filesystem::path> glob_path(const std::string& basepath,
												 const std::initializer_list<std::string>& pathnames)
	{
		return glob_path(basepath, std::vector<std::string>(pathnames));
	}
	std::vector<std::filesystem::path> rglob(const std::initializer_list<std::string>& pathnames)
	{
		return rglob(std::vector<std::string>(pathnames));
	}
	std::vector<std::filesystem::path> rglob_path(const std::string& basepath,
												  const std::initializer_list<std::string>& pathnames)
	{
		return rglob_path(basepath, std::vector<std::string>(pathnames));
	}
} // namespace glob

#endif
