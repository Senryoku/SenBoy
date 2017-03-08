#include "Cartridge.hpp"

#include <cstring> // memset
#include <sstream>
#include <chrono>		// RTC
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>

#include <Tools/Config.hpp>

Cartridge::Cartridge(const std::string& path)
{
	load(path);
}
	
bool Cartridge::load(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);

	if(!file)
	{
		if(Log)
			Log("Error: '" + path + "' could not be opened.");
		return false;
	}
	
	_data.assign(std::istreambuf_iterator<byte_t>(file),
				std::istreambuf_iterator<byte_t>());
			
	if(Log)	
		Log("Loaded '" + path + "'");
	
	return init();
}

bool Cartridge::load_from_memory(const unsigned char data[], size_t size)
{
	_data = std::vector<byte_t>(data, data + size);
	if(Log)
		Log("Loaded a ROM from memory");
	return init();
}

void Cartridge::reset()
{
	_rom_bank = 1;
	_ram_bank = 0;
	_enable_ram = false;
	_ram_size = 0;
	_ram.clear();
	_mode = 0;
	std::memset(_rtc_registers, 0, 5);
}

bool Cartridge::init()
{
	reset();
	if(!(isMBC1() || isMBC3() || isMBC5()))
	{
		if(Log)
			Log("Error: Cartridge format " + Hexa8(getType()).str() + " not supported!");
		return false;
	}

	_ram_size = getRAMSize();
	
	if(Log)
	{
		Log(" Title: '" + getName() +
			"', Size: " + std::to_string(_data.size()) + "B (" + Hexa8(*(_data.data() + ROMSize)).str() +
			"), RAM Size: " + std::to_string(_ram_size) + "B (" + Hexa8(*(_data.data() + RAMSize)).str() +
			"), Type: " + Hexa8(getType()).str() +
			", Battery: " + (hasBattery() ? "Yes" : "No"));

		switch(getCGBFlag())
		{
			case No: Log(" No Color GameBoy support."); break;
			case Partial: Log(" Partial Color GameBoy support."); break;
			case Only: Log(" Color GameBoy only ROM."); break;
		}
	}

	if(_ram_size == 0 && hasRAM())
	{
		if(Log)
			Log(" Warning: ROM claim to have RAM without specifying size... Using 128kB.");
		_ram_size = 128 * 1024;
	}

	if(_ram_size > 0)
	{
		// Search for a saved RAM
		if(hasBattery() && file_exists(save_path()))
		{
			if(Log)
				Log("Found a save file, loading it... ");
			std::ifstream save(save_path(), std::ios::binary);
			_ram.assign(std::istreambuf_iterator<byte_t>(save),
						std::istreambuf_iterator<byte_t>());
			if(Log)
				Log("Done.");
		} else {
			_ram.clear();
			_ram.resize(_ram_size, 0);
		}
	}
	return true;
}

byte_t Cartridge::read(addr_t addr) const
{
	if(addr < 0x4000) // ROM Bank 0
	{
		//assert(addr < _data.size());
		if(addr < _data.size())
			return _data[addr];
		else
			return 0;
	} else if(addr < 0x8000) { // Switchable ROM Bank
		if(isMBC1() || isMBC2() || isMBC3())
		{
			size_t a = addr + static_cast<size_t>((rom_bank() & 0x7F) - 1) * 0x4000;
			if(a < _data.size())
				return _data[a];
			else
				return 0;
		} else if(isMBC5()) {
			return _data[addr + ((rom_bank() & 0x1FF) - 1) * 0x4000];
		}
	} else if(addr >= 0xA000 && addr < 0xC000) { // Switchable RAM Bank
		if(isMBC1() || isMBC5())
		{
			size_t a = static_cast<size_t>(ram_bank() * 0x2000 + (addr & 0x1FFF));
			if(a < _ram_size)
				return _ram[a];
			else
				return 0;
		} else if(isMBC3()) {
			if(_ram_bank >= 0x8 && _ram_bank <= 0xC)
				return _rtc_registers[_ram_bank - 0x8];
			else
				return _ram[_ram_bank * 0x2000 + (addr & 0x1FFF)];
		} else if(isMBC2()) {
			return _ram[addr & 0x1FF];
		}
	}

	Log("Error: Wrong address queried to the Cartridge: " + Hexa(addr).str());
	return 0;
}

void Cartridge::write_ram(addr_t addr, byte_t value)
{
	if(_enable_ram)
	{
		assert(!_ram.empty());
		assert(static_cast<size_t>(ram_bank() * 0x2000 + (addr & 0x1FFF)) < _ram_size);
		_ram[ram_bank() * 0x2000 + (addr & 0x1FFF)] = value;
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
			if(isMBC1()) // lower 5 bits of the ROM Bank
			{
				value &= 0x1F;
				if(value == 0) value = 1;
				_rom_bank = value;
			} else if(isMBC2()) {
				_rom_bank = (value & 0x0F);
			} else if(isMBC3()) {
				if(value == 0) value = 1;
				value &= 0x7F;
				_rom_bank = value;
			} else if(isMBC5()) {
				if(addr < 0x3000) {	// Bits 0-7
					_rom_bank = (_rom_bank & 0x100) | (value & 0xFF);
				} else {			// Bit 8
					if(getROMBankCount() >= 0x100)
						_rom_bank = (_rom_bank & 0x0FF) | ((value & 0x01) ? 0x100 : 0);
				}
				assert(_rom_bank < 0x1E0 && _rom_bank < getROMBankCount());
			}
		break;

		case 0x4000:
		case 0x5000:
			if(isMBC1())
			{
				_ram_bank = value & 3; // Select RAM bank or upper bits of ROM Bank
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
			Log("Error: Write on Cartridge on " + Hexa(addr).str());
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
	return config::to_abs(ss.str());
}

void Cartridge::save() const
{
	if(!hasBattery())
		return;

	if(Log)
		Log("Saving RAM to '" + save_path() + "'... ");
	std::ofstream save(save_path(), std::ios::binary | std::ios::trunc);
	save.write(_ram.data(), _ram.size());
	if(Log)
		Log("Done.");
}
