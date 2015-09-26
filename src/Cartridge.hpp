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
		
		_ram_size = getRAMSize();
		if(_ram_size > 0)
			_ram = new byte_t[_ram_size];
		
		std::cout << "Loaded '" << path << "' : " << getName() << std::endl;
		std::cout << " (Size: " << std::dec << _data.size() << 
					", RAM Size: " << _ram_size << 
					", Type: " << std::hex << getType() << 
					")" << std::endl;
		
		if(getType() > 0x03)
		{
			std::cerr << "Cartridge format not supported! Exiting..." << std::endl;
			exit(1);
		}
		
		return true;
	}
	
	byte_t read(addr_t addr) const
	{
		assert(addr >= 0x4000 || _data.size() > addr);
		if(addr < 0x4000) // ROM Bank 0
			return _data[addr];
		else if(addr < 0x8000) // Switchable ROM Bank
			return _data[addr + ((_rom_bank & 0x1F) - 1) * 0x4000];
		else if(addr >= 0xA000 && addr < 0xC000) // Switchable RAM Bank
			return _ram[_ram_bank * 0x2000 + (addr & 0x1FFF)];
		
		std::cerr << "Error: Wrong address queried to the Cartridge: 0x" << std::hex << addr << std::endl;
		return _data[0x0000]; // Dummy
	}
	
	void write_ram(addr_t addr, byte_t value)
	{
		if(_enable_ram)
		{
			assert(_ram != nullptr);
			assert(_ram_bank * 0x2000 + (addr & 0x1FFF) < _ram_size);
			_ram[_ram_bank * 0x2000 + (addr & 0x1FFF)] = value;
		}
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
					if(_ram_bank * 0x2000 > _ram_size)
						_ram_bank = _ram_size / 0x2000 - 1;
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
				write_ram(addr, value);
			break;
			default:
				std::cerr << "Error: Write on Cartridge on 0x" << std::hex << addr << std::endl;
			break;
		}
	}
	
	inline std::string getName() { return std::string(_data.data() + 0x0134, 15); }
	inline int getType() { return *(_data.data() + 0x0147); }
	
	inline size_t getROMSize()
	{ 
		size_t s = *(_data.data() + 0x0148);
		if(s < 0x08) return (32 * 1024) << s;
		// @todo Handle more cases
		return 32 * 1024;
	}
	
	inline size_t getRAMSize()
	{ 
		switch(*(_data.data() + 0x0149))
		{
			default:
			case 0x00: return 0; break;
			case 0x01: return 2 * 1024; break;
			case 0x02: return 8 * 1024; break;
			case 0x03: return 32 * 1024; break;
			case 0x04: return 128 * 1024; break;
			case 0x05: return 64 * 1024; break;
		}
	}
	
private:
	std::vector<byte_t> _data;
	byte_t*				_ram = nullptr;
	
	size_t		_rom_bank = 1;
	size_t		_ram_bank = 0;
	size_t		_enable_ram = false;
	size_t		_ram_size = 0;
	byte_t		_mode = 0;
};
