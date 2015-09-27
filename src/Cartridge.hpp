#pragma once

#include <string>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

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
		save();
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
		
		if(getType() > 0x03)
		{
			std::cerr << "Error: Cartridge format " << getType() 
					<< " not supported! Exiting..." << std::endl;
			exit(1);
		}
		
		_ram_size = getRAMSize();
		
		std::cout << "Loaded '" << path << "' : " << getName() << std::endl;
		std::cout << " (Size: " << std::dec << _data.size() << 
					", RAM Size: " << _ram_size << 
					", Type: " << std::hex << getType() << 
					", Battery: " << (hasBattery() ? "Yes" : "No") <<
					")" << std::endl;

		if(_ram_size > 0)
		{
			// Search for a saved RAM
			if(hasBattery() && file_exists(save_path()))
			{
				std::cout << "Found a save file, loading it... ";
				std::ifstream save(save_path(), std::ios::binary);
				_ram.assign(std::istreambuf_iterator<byte_t>(save), 
							std::istreambuf_iterator<byte_t>());
				std::cout << "Done." << std::endl;
			} else {
				_ram.resize(_ram_size);
			}
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
			assert(!_ram.empty());
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
	
	inline std::string getName() const
	{
		return std::string(_data.data() + 0x0134, 15);
	}

	inline unsigned int getType() const 
	{
		return *(_data.data() + 0x0147);
	}
	
	inline size_t getROMSize() const
	{ 
		size_t s = *(_data.data() + 0x0148);
		if(s < 0x08) return (32 * 1024) << s;
		// @todo Handle more cases
		return 32 * 1024;
	}
	
	inline size_t getRAMSize() const
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

	inline bool hasBattery() const
	{
		/// @todo Support more types.
		return getType() == 0x03;
	}

	/**
	 * Saves RAM to a file if a battery is present
	**/
	void save() const
	{
		if(!hasBattery())
			return;

		std::cout << "Saving RAM... ";
		std::ofstream save(save_path(), std::ios::binary | std::ios::trunc);
		save.write(_ram.data(), _ram.size());
		std::cout << "Done." << std::endl;
	}

	inline std::string save_path() const
	{
		std::string r, n = getName();
		n.erase(remove_if(n.begin(), n.end(), [](char c) { return !(c >= 48 && c < 122); }), n.end()); 
		r = "saves/";
		r.append(n);
		r.append(".sav");
		return r;
	}
	
private:
	std::vector<byte_t> _data;
	std::vector<byte_t> _ram;
	
	size_t		_rom_bank = 1;
	size_t		_ram_bank = 0;
	size_t		_enable_ram = false;
	size_t		_ram_size = 0;
	byte_t		_mode = 0;

	inline bool file_exists(const std::string& path)
	{
		struct stat buffer;   
		return (stat (path.c_str(), &buffer) == 0); 
	}
};
