
#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace glob {

/// \param pathname string containing a path specification
/// \return vector of paths that match the pathname
///
/// Pathnames can be absolute (/usr/src/Foo/Makefile) or relative (../../Tools/*/*.gif)
/// Pathnames can contain shell-style wildcards
/// Broken symlinks are included in the results (as in the shell)
std::vector<std::filesystem::path> glob(const std::string &pathname);

/// \param basepath the root directory to run in
/// \param pathname string containing a path specification
/// \return vector of paths that match the pathname
///
/// Pathnames can be absolute (/usr/src/Foo/Makefile) or relative (../../Tools/*/*.gif)
/// Pathnames can contain shell-style wildcards
/// Broken symlinks are included in the results (as in the shell)
std::vector<std::filesystem::path> glob_path(const std::string& basepath, const std::string& pathname);

/// \param pathnames string containing a path specification
/// \return vector of paths that match the pathname
///
/// Globs recursively.
/// The pattern “**” will match any files and zero or more directories, subdirectories and
/// symbolic links to directories.
std::vector<std::filesystem::path> rglob(const std::string &pathname);

/// \param basepath the root directory to run in
/// \param pathnames string containing a path specification
/// \return vector of paths that match the pathname
///
/// Globs recursively.
/// The pattern “**” will match any files and zero or more directories, subdirectories and
/// symbolic links to directories.
std::vector<std::filesystem::path> rglob_path(const std::string& basepath, const std::string& pathname);


/// Runs `glob` against each pathname in `pathnames` and accumulates the results
std::vector<std::filesystem::path> glob(const std::vector<std::string> &pathnames);

/// Runs `glob` against each pathname in `pathnames` and accumulates the results
std::vector<std::filesystem::path> glob_path(const std::string& basepath, const std::vector<std::string> &pathnames);


/// Runs `rglob` against each pathname in `pathnames` and accumulates the results
std::vector<std::filesystem::path> rglob(const std::vector<std::string> &pathnames);

/// Runs `rglob` against each pathname in `pathnames` and accumulates the results
std::vector<std::filesystem::path> rglob_path(const std::string& basepath, const std::vector<std::string>& pathnames);

/// Initializer list overload for convenience
std::vector<std::filesystem::path> glob(const std::initializer_list<std::string> &pathnames);

/// Initializer list overload for convenience
std::vector<std::filesystem::path> glob_path(const std::string& basepath, const std::initializer_list<std::string>& pathnames);


/// Initializer list overload for convenience
std::vector<std::filesystem::path> rglob(const std::initializer_list<std::string> &pathnames);

/// Initializer list overload for convenience
std::vector<std::filesystem::path> rglob_path(const std::string& basepath, const std::initializer_list<std::string>& pathnames);

} // namespace glob