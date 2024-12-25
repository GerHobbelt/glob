
#pragma once
#include <string>
#include <vector>
#include <functional>
#include <any>
#include <regex>
#include <string_view>

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

// https://stackoverflow.com/questions/1537964/visual-c-equivalent-of-gccs-attribute-packed

#if defined(_MSC_VER)
#define PACK( __Declaration__ ) __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#else // __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
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
	std::vector<std::string> pathnames;		 // a set of paths to scan (MAY contain wildcards at both directory and file level, e.g. `/bla/**/sub*.dir/*` or `/bla/foo*.pdf`)
	std::vector<int> max_recursion_depth;  // -1 means: unlimited depth

	bool include_hidden_entries = false;   // include files/directories which start with a '.' dot
	bool include_matching_directories = false;   // include directories which match the last wildcard, e.g. when searching for "*.pdf" match a directory named "collection.pdf/"
	bool include_matching_files = true;          // include files which match the last wildcard, e.g. when searching for "*.pdf" match a file named "article.pdf"

	//bool follow_symlinks = true;    <-- userland code can call fs::weak_canonical(path) on all entries instead.

	// --------------------------------------------------------------------------------------

#if 0
	// user-provided optional object that is passed around and can be used to track/memoize metadata related to the glob activity.
	std::any user_obj;
#endif

	// rather than provide a bunch of std::function-based callbacks, we use virtual member functions, which the user can override in their overridden options class.
	// This is done for performance reasons and to save space for the usual expected use cases. It also allows userland code to have easy access to userland-defined
	// data and/or track/register additional glob metadata in any way they like, without having to revert to providing a somewhat-hacky std::any user object reference
	// above...

	PACK(struct filter_state_t {
		bool accept: 1;								// false = reject, i.e. do not add to the result set.
		bool recurse_into: 1;
		bool stop_scan_for_this_spec: 1;
		bool do_report_progress: 1;		// side-effect of the custom filter action: this provides a most flexible way to decide when and how often to invoke the progress reporting callback method.
	});
	typedef struct filter_state_t filter_state_t;

	struct filter_info_t {
		fs::path basepath;
		fs::path filename;

		fs::path matching_wildcarded_fragment;
		fs::path subsearch_spec;

		bool fragment_is_wildcarded;
		bool fragment_is_double_star;

		bool userland_may_override_accept;
		bool userland_may_override_recurse_into;

		bool is_directory;
		bool is_hidden;

		int depth;
		int max_recursion_depth;

		int search_spec_index;
	};

	// filter callback: returns pass/reject for given path; this can override the default glob reject/accept logic in either direction
	// as both rejected and accepted entries are fed to this callback method.
	virtual filter_state_t filter(fs::path path, filter_state_t glob_says_pass, const filter_info_t &info);

	struct progress_info_t {
		fs::path current_path;
		filter_info_t path_filter_info;
		filter_state_t path_filter_state;

		int item_count_scanned;
		int dir_count_scanned;

		int searchpath_queue_index;
		int searchpath_queue_count;

		int search_spec_index;
		int search_spec_count;
	};
	// progress callback: shows currently processed path, pass/reject status and progress/scan completion estimate.
	// Return `false` to abort the glob action.
	virtual bool progress_reporting(const progress_info_t &info);

	// --------------------------------------------------------------------------------------

	void init_max_recursion_depth_set(bool recursive_search = true, int default_search_depth = 1)
	{
		max_recursion_depth.resize(pathnames.size());
		for (int index = 0; index < pathnames.size(); index++) {
			max_recursion_depth[index] = (recursive_search ? -1 : default_search_depth);
		}
	};

	options(const fs::path& basepath, std::vector<std::string> pathnames, bool recursive_search = true)
		: basepath(basepath),
		pathnames(pathnames)
	{
		init_max_recursion_depth_set(recursive_search);
	};
	/// \param basepath the root directory to run in
	/// \param pathname string containing a path specification
	/// Convenience constructor for use when only a single pathspec will be used
	options(const fs::path& basepath, const std::string& pathname, bool recursive_search = true)
		: basepath(basepath),
		pathnames({pathname})
	{
		init_max_recursion_depth_set(recursive_search);
	};

	virtual ~options() = default;
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

std::vector<fs::path> glob(const options &search_specification);
std::vector<fs::path> glob(options &search_specification);

/// Helper function: expand '~' HOME part (when used in the path) and normalize the given path.
fs::path expand_and_normalize_tilde(fs::path path);

std::string translate_pattern(std::string_view pattern);
std::regex compile_pattern(std::string_view pattern);
bool fnmatch(std::string&& name,const std::regex& pattern);
std::vector<fs::path> filter(const std::vector<fs::path> &names,std::string_view pattern);

fs::path mk_relative(const fs::path& p,const fs::path& base);

} // namespace glob
