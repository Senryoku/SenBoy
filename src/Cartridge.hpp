#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <vector>

/**
 * GameBoy Cartridge
**/
class Cartridge
{
public:
	using byte_t = char;
	using addr_t = uint16_t;
	
	Cartridge()
	{
	}
	
	Cartridge(const std::string& path)
	{
		load(path);
	}
	
	~Cartridge()
	{
		delete[] _ram;
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
					
		_ram = new byte_t[getRAMSize()];
		
		std::cout << "Loaded '" << path << "' : " << getName() << std::endl;
		std::cout << " (Size: " << _data.size() << ", Type: " << std::hex << getType() << ")" << std::endl;
		
		if(getType() > 0x01)
		{
			std::cerr << "Cartridge format not supported! Exiting..." << std::endl;
			exit(1);
		}
		
		return true;
	}
	
	byte_t read(addr_t addr) const
	{
		if(addr < 0x4000) // ROM Bank 0
			return _data[addr];
		else if(addr < 0x8000) // Switchable ROM Bank
			return _data[addr + (_rom_bank - 1) * 0x4000];
		else if(addr >= 0xA000 && addr < 0xC000) // Switchable RAM Bank
			return _ram[_rom_bank * 0x4000 + (addr & 0x1FFF)];
		
		std::cerr << "Error: Wrong address queried to the Cartridge: 0x" << std::hex << addr << std::endl;
		return _data[0x0100]; // Dummy
	}
	
	byte_t& rw(addr_t addr)
	{
		if(addr < 0x4000) // ROM Bank 0
			return _data[addr];
		else if(addr < 0x8000) // Switchable ROM Bank
			return _data[addr + (_rom_bank - 1) * 0x4000];
		else if(addr >= 0xA000 && addr < 0xC000) // Switchable RAM Bank
			return _ram[_rom_bank * 0x4000 + (addr & 0x1FFF)];
		
		std::cerr << "Error: Wrong address queried to the Cartridge: 0x" << std::hex << addr << std::endl;
		return _data[0x0100]; // Dummy
	}
	
	void write(addr_t addr, byte_t value)
	{
		// MBC1 only for now
		switch(addr & 0xF000)
		{
			// External RAM Switch
			case 0x0000:
			case 0x1000:
				_enable_ram = ((value & 0x0F) == 0x0A) ? 1 : 0;
			break;
			
			// Select ROM bank (5 low bits)
			case 0x2000:
			case 0x3000:
				value &= 0x1F;
				if(value == 0) value = 1;
				_rom_bank = (_rom_bank & 0x60) + value;
			break;
			
			case 0x4000:
			case 0x5000:
				if(_mode)
				{
					_ram_bank = value & 3; // Select RAM bank
				} else {
					_rom_bank = (_rom_bank & 0x1F) + ((value & 3) << 5); // Select ROM bank (3 high bits)
				}
			break;
			
			// Mode Switch
			case 0x6000:
			case 0x7000:
				_mode = value & 1;
			break;
			
			// External RAM
			case 0xA000:
			case 0xB000:
				_ram[_ram_bank * 0x4000 + (addr & 0x1FFF)] = value;
			break;
		}
	}
	
	inline std::string getName() { return std::string(_data.data() + 0x0134, 15); }
	inline int getType() { return *(_data.data() + 0x0147); }
	inline size_t getRAMSize() { return *(_data.data() + 0x0149); }

private:
	std::vector<byte_t> _data;
	byte_t*				_ram = nullptr;
	
	size_t		_rom_bank = 1;
	size_t		_ram_bank = 1;
	size_t		_enable_ram = false;
	byte_t		_mode = 0;
};
