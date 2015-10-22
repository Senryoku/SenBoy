#pragma once

#include <string>

namespace config
{
	extern std::string _executable_folder;

	// 'Parsing' executable location
	void set_folder(const char* exec_path);
	std::string to_abs(const std::string& path);
}
