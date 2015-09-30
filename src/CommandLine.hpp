#pragma once

#include <algorithm>

/**
 * @file Simplistic parsing of command line arguments.
 *
 * -x for switch options (bool)
 * $x for options with one argument
**/

/// @return First argument that is not an option or an option argument
char* get_file(int argc, char* argv[])
{
	for(int i = 1; i < argc; ++i)
	{
		if(argv[i][0] == '$')
			++i;
		else if(argv[i][0] != '-')
			return argv[i];
	}
	return nullptr;
}

/// @return True if opt is present, false otherwise.
bool has_option(int argc, char* argv[], const std::string& opt)
{
	return std::find(argv + 1, argv + argc, opt) != argv + argc;
}

/// @return Argument following opt if found, nullptr otherwise.
char* get_option(int argc, char* argv[], const std::string& opt)
{
	char** s = std::find(argv + 1, argv + argc, opt);
	if(s != argv + argc && ++s != argv + argc)
		return *s;
	return nullptr;
}