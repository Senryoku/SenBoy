#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <vector>

/**
 * GameBoy Cartridge
**/
class ROM
{
public:
	using byte_t = char;
	using addr_t = uint16_t;
	
	ROM()
	{
	}
	
	ROM(const std::string& path)
	{
		load(path);
	}
	
	~ROM()
	{
	}
	
	bool load(const std::string& path)
	{
		std::ifstream file(path, std::ios::binary);
		
		if(!file)
		{
			std::cerr << "Error: '" << path << "' could not be opened." << std::endl;
			return false;
		}
		
		_data.assign(std::istreambuf_iterator<byte_t>(file), 
					std::istreambuf_iterator<byte_t>());
					
		std::cout << "Loaded '" << path << "' : " << getName() << std::endl;
		std::cout << " (Size: " << _data.size() << ", Type: " << std::hex << getType() << ")" << std::endl;
		
		return true;
	}
	
	byte_t read(addr_t addr) const
	{
		return _data[addr % _data.size()];
	}
	
	void write(addr_t addr, byte_t value)
	{
		/// @todo Memory Banks !
	}
	
	std::string getName() { return std::string(_data.data() + 0x0134, 15); }
	int getType() { return *(_data.data() + 0x0134 + 19); }

private:
	std::vector<byte_t> _data;
};
