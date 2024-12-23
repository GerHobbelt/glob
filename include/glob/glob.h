
#pragma once
#include <string>
#include <vector>

#if !defined(GLOB_USE_GHC_FILESYSTEM) || defined(GHC_DO_NOT_USE_STD_FS)
#if !__has_include(<filesystem>) || defined(GHC_DO_NOT_USE_STD_FS)
#define GLOB_USE_GHC_FILESYSTEM   1
#ifndef GHC_DO_NOT_USE_STD_FS
#define GHC_DO_NOT_USE_STD_FS     1
#endif
#endif
#else
#define GHC_DO_NOT_USE_STD_FS     1
#endif

// to prevent mishaps, use the GHC-provided filesystem selector headerfile: ghc/fs_std.hpp
// which listens to both system availability of std::filesystem and the GHC_DO_NOT_USE_STD_FS
// overriding switch/define.
#if 0

#include <ghc/filesystem.hpp>
#if __has_include(<filesystem>)
#include <filesystem>
#endif

#else

#include <ghc/fs_std.hpp>  // namespace fs = std::filesystem;   or   namespace fs = ghc::filesystem;

#endif

namespace glob {

#if defined(GHC_USE_GHC_FS)
namespace fs = ghc::filesystem;
#else
namespace fs = std::filesystem;
#endif


/// Helper struct for extended options
struct options {
	fs::path basepath;
	std::vector<std::string> pathnames;
	std::vector<int> max_recursion_depth;  // -1 means: unlimited depth
	bool include_hidden_entries = false;

	// filter regexes / wildcards: both a positive (pass) and a negative (reject) set.
	std::vector<std::string> accepts;
	std::vector<std::string> rejects;

	// filter callback: returns pass/reject for given path

	// progress callback: shows currently processed path, pass/reject status and progress/scan completion estimate.

	options(const fs::path& basepath, std::vector<std::string> pathnames)
		: basepath(basepath),
		pathnames(pathnames)
	{};
	/// \param basepath the root directory to run in
	/// \param pathname string containing a path specification
	/// Convenience constructor for use when only a single pathspec will be used
	options(const fs::path& basepath, const std::string& pathname)
		: basepath(basepath),
		pathnames({pathname})
	{};
};

/// Helper struct for extended file/path attributes
struct path_w_extattr : public fs::path {
	bool is_hidden = false;
	bool is_readonly = false;
	bool is_executable = false;

	int path_depth = 0;

	path_w_extattr(const fs::path &pathspec):
		fs::path(pathspec) {
	}

	path_w_extattr(const fs::path &&pathspec) :
		fs::path(pathspec) {
	}
};

/// Helper struct for extended glob results' storage
struct results {
	fs::path basepath;
	std::vector<path_w_extattr> pathnames;
};

/// \param pathname string containing a path specification
/// \return vector of paths that match the pathname
///
/// Pathnames can be absolute (/usr/src/Foo/Makefile) or relative (../../Tools/*/*.gif)
/// Pathnames can contain shell-style wildcards
/// Broken symlinks are included in the results (as in the shell)
std::vector<fs::path> glob(const std::string &pathname);

/// \param basepath the root directory to run in
/// \param pathname string containing a path specification
/// \return vector of paths that match the pathname
///
/// Pathnames can be absolute (/usr/src/Foo/Makefile) or relative (../../Tools/*/*.gif)
/// Pathnames can contain shell-style wildcards
/// Broken symlinks are included in the results (as in the shell)
std::vector<fs::path> glob_path(const std::string& basepath, const std::string& pathname);

/// \param pathnames string containing a path specification
/// \return vector of paths that match the pathname
///
/// Globs recursively.
/// The pattern “**” will match any files and zero or more directories, subdirectories and
/// symbolic links to directories.
std::vector<fs::path> rglob(const std::string &pathname);

/// \param basepath the root directory to run in
/// \param pathnames string containing a path specification
/// \return vector of paths that match the pathname
///
/// Globs recursively.
/// The pattern “**” will match any files and zero or more directories, subdirectories and
/// symbolic links to directories.
std::vector<fs::path> rglob_path(const std::string& basepath, const std::string& pathname);

/// Runs `glob` against each pathname in `pathnames` and accumulates the results
std::vector<fs::path> glob(const std::vector<std::string> &pathnames);

/// Runs `glob` against each pathname in `pathnames` and accumulates the results
std::vector<fs::path> glob_path(const std::string& basepath, const std::vector<std::string> &pathnames);

/// Runs `rglob` against each pathname in `pathnames` and accumulates the results
std::vector<fs::path> rglob(const std::vector<std::string> &pathnames);

/// Runs `rglob` against each pathname in `pathnames` and accumulates the results
std::vector<fs::path> rglob_path(const std::string& basepath, const std::vector<std::string>& pathnames);

/// Initializer list overload for convenience
std::vector<fs::path> glob(const std::initializer_list<std::string> &pathnames);

/// Initializer list overload for convenience
std::vector<fs::path> glob_path(const std::string& basepath, const std::initializer_list<std::string>& pathnames);

/// Initializer list overload for convenience
std::vector<fs::path> rglob(const std::initializer_list<std::string> &pathnames);

/// Initializer list overload for convenience
std::vector<fs::path> rglob_path(const std::string& basepath, const std::initializer_list<std::string>& pathnames);

results glob(const options &search_specification);

/// Helper function: expand '~' HOME part (when used in the path) and normalize the given path.
fs::path expand_and_normalize_tilde(fs::path path);

} // namespace glob
