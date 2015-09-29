#include "Cartridge.hpp"

#include <sstream>
#include <chrono>		// RTC
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>

Cartridge::Cartridge(const std::string& path)
{
	load(path);
}

Cartridge::~Cartridge()
{
	save();
}
	
bool Cartridge::load(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);

	if(!file)
	{
		std::cerr << "Error: '" << path << "' could not be opened." << std::endl;
		return false;
	}

	_data.assign(std::istreambuf_iterator<byte_t>(file),
				std::istreambuf_iterator<byte_t>());

	if(!(isMBC1() || isMBC3() || isMBC5()))
	{
		std::cerr << "Error: Cartridge format 0x" << std::hex << getType()
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
				
	switch(getCGBFlag())
	{
		case No: std::cout << "No Color GameBoy support." << std::endl; break;
		case Partial: std::cout << "Partial Color GameBoy support." << std::endl; break;
		case Only: std::cout << "Color GameBoy only ROM." << std::endl; break;
	}

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
			_ram.resize(_ram_size, 0);
		}
	}

	return true;
}

byte_t Cartridge::read(addr_t addr) const
{
	assert(addr >= 0x4000 || _data.size() > addr);
			
	if(addr < 0x4000) // ROM Bank 0
	{
		return _data[addr];
	} else if(addr < 0x8000) { // Switchable ROM Bank
		if(isMBC1() || isMBC2())
			return _data[addr + ((_rom_bank & 0x1F) - 1) * 0x4000];
		else if(isMBC3())
			return _data[addr + ((_rom_bank & 0x7F) - 1) * 0x4000];
		else if(isMBC5())
			return _data[addr + ((_rom_bank & 0x1FF) - 1) * 0x4000];
	} else if(addr >= 0xA000 && addr < 0xC000) { // Switchable RAM Bank
		if(isMBC1() || isMBC5())
		{
			return _ram[_ram_bank * 0x2000 + (addr & 0x1FFF)];
		} else if(isMBC3()) {
			if(_ram_bank >= 0x8 && _ram_bank <= 0xC)
				return _rtc_registers[_ram_bank - 0x8];
			else
				return _ram[_ram_bank * 0x2000 + (addr & 0x1FFF)];
		} else if(isMBC2()) {
			return _ram[addr & 0x1FF];
		}
	}

	std::cerr << "Error: Wrong address queried to the Cartridge: 0x" << std::hex << addr << std::endl;
	return _data[0x0000]; // Dummy
}

void Cartridge::write_ram(addr_t addr, byte_t value)
{
	if(_enable_ram)
	{
		assert(!_ram.empty());
		assert(_ram_bank * 0x2000 + (addr & 0x1FFF) < _ram_size);
		_ram[_ram_bank * 0x2000 + (addr & 0x1FFF)] = value;
	}
}

void Cartridge::write(addr_t addr, byte_t value)
{
	// MBC1/MBC3/MBC5 only for now
	switch(addr & 0xF000)
	{
		// External RAM Switch
		case 0x0000:
		case 0x1000:
			_enable_ram = ((value & 0x0F) == 0x0A);
		break;

		// Select ROM bank (5 low bits)
		case 0x2000:
		case 0x3000:
			if(isMBC1())
			{
				value &= 0x1F; // MBC1
				if(value == 0) value = 1;
				_rom_bank = (_rom_bank & 0x60) + value;
			} else if(isMBC2()) {
				_rom_bank = (value & 0x0F);
			} else if(isMBC3()) {
				if(value == 0) value = 1;
				value &= 0x7F; // MBC3
				_rom_bank = value;
			} else if(isMBC5()) {
				if(addr < 0x3000)
					_rom_bank = (_rom_bank & 0x100) | (value & 0xFF);
				else 
					_rom_bank = (_rom_bank & 0x0FF) | (((size_t(value) & 0x01) << 8) & 0x100);
				assert(_rom_bank < 0x1E0);
			}
		break;

		case 0x4000:
		case 0x5000:
			if(isMBC1())
			{
				if(_mode)
				{
					_ram_bank = value & 3; // Select RAM bank
					if(_ram_bank * 0x2000 > _ram_size)
						_ram_bank = _ram_size / 0x2000 - 1;
				} else {
					_rom_bank = (_rom_bank & 0x1F) + ((value & 3) << 5); // Select ROM bank (2 high bits)
				}
			} else if(isMBC3()) {
				_ram_bank = value; // Select RAM bank OR RTC Register
			} else if(isMBC5()) {
				_ram_bank = value & 0x0F;
			}
		break;

		// Mode Switch
		case 0x6000:
		case 0x7000:
			if(isMBC1())
			{
				_mode = value & 1;
			} else if(isMBC3()) {
				static bool zero_received = false;
				if(value == 0)
				{
					zero_received = true;
				} else if(zero_received && value == 1) {
					latch_clock_data();
					zero_received = false;
				} else {
					zero_received = false;
				}
			}
		break;

		// External RAM
		case 0xA000:
		case 0xB000:
			if(isMBC1() || isMBC2() || _ram_bank <= 0x3 || isMBC5())
				write_ram(addr, value);
			else if(isMBC3())
				_rtc_registers[_ram_bank - 0x8] = value;
		break;
		default:
			std::cerr << "Error: Write on Cartridge on 0x" << std::hex << addr << std::endl;
		break;
	}
}

void Cartridge::latch_clock_data()
{
	auto t = std::chrono::system_clock::now();
	time_t tt = std::chrono::system_clock::to_time_t(t);
	tm l_tm = *localtime(&tt);
	_rtc_registers[0] = l_tm.tm_sec;
	_rtc_registers[1] = l_tm.tm_min;
	_rtc_registers[2] = l_tm.tm_hour;
	size_t days = l_tm.tm_yday; //... Wep. that's wrong.
	_rtc_registers[3] = days & 0xFF;
	_rtc_registers[4] = (days & 0x1FF) >> 8;
	/// @todo Complete
}

bool Cartridge::file_exists(const std::string& path)
{
	struct stat buffer;
	return (stat (path.c_str(), &buffer) == 0);
}

std::string Cartridge::save_path() const
{
	if(_data.empty()) return "";
	std::string n = getName();
	n.erase(remove_if(n.begin(), n.end(), [](char c) { return !(c >= 48 && c < 122); }), n.end());
	std::stringstream ss;
	ss << "saves/";
	ss << n;
	ss << '_';
	ss << std::hex << getHeaderChecksum();
	ss << '_';
	ss << std::hex << getChecksum();
	ss << ".sav";
	return ss.str();
}

void Cartridge::save() const
{
	if(!hasBattery())
		return;

	std::cout << "Saving RAM... ";
	std::ofstream save(save_path(), std::ios::binary | std::ios::trunc);
	save.write(_ram.data(), _ram.size());
	std::cout << "Done." << std::endl;
}
