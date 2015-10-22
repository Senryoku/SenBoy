#include "Config.hpp"

namespace config
{
	std::string _executable_folder;
	
	void set_folder(const char* exec_path)
	{
		_executable_folder = exec_path;
		size_t last = _executable_folder.find_last_of('/');
		if(last == std::string::npos)
			last = _executable_folder.find_last_of('\\');
		if(last == std::string::npos)
			_executable_folder = "./";
		else
			_executable_folder.resize(last + 1);
	}

	std::string to_abs(const std::string& path)
	{
		std::string r = _executable_folder;
		r += path;
		return r;
	}
}
