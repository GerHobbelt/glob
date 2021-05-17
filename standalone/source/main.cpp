#include <glob/glob.h>
#include <glob/version.h>

#include <clipp.h>
#include <iostream>
#include <string>
#include <set>

int main(int argc, const char** argv)
{
	using namespace clipp;

	bool recursive = false;
	std::vector<std::string> patterns;
	std::set<std::string> tags;

	enum class mode { none, help, version, glob };
	mode selected = mode::none;

	auto options = (
		option("-r", "--recursive").set(recursive) % "Run glob recursively",
		repeatable( option("-i", "--input").set(selected, mode::glob) & values("patterns", patterns) ) % "Patterns to match"
	);
	auto cli = (
		(options
		| command("-h", "--help").set(selected, mode::help) % "Show this screen."
		| command("-v", "--version").set(selected, mode::version) % "Show version."
		)
	);

	auto help = [cli]()
	{
		std::cout << make_man_page(cli, "glob")
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

	if (recursive)
	{
		for (auto& match : glob::rglob(patterns))
		{
			std::cout << match << "\n";
		}
	}
	else
	{
		for (auto& match : glob::glob(patterns))
		{
			std::cout << match << "\n";
		}
	}

	return EXIT_SUCCESS;
}
