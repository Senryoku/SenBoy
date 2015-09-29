#pragma once

#include <string>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <vector>
#include <cassert>

#include "Common.hpp"

/**
 * GameBoy Cartridge
**/
class Cartridge
{
public:	
	enum Type : ubyte_t
	{
		ROM						 = 0x00,
		MBC1					 = 0x01,
		MBC1_RAM				 = 0x02,
		MBC1_RAM_BATTERY		 = 0x03,
		MBC2					 = 0x05,
		MBC2_BATTERY			 = 0x06,
		ROM_RAM					 = 0x08,
		ROM_RAM_BATTERY			 = 0x09,
		MMM01					 = 0x0B,
		MMM01_RAM				 = 0x0C,
		MMM01_RAM_BATTERY		 = 0x0D,
		MBC3_TIMER_BATTERY		 = 0x0F,
		MBC3_TIMER_RAM_BATTERY	 = 0x10,
		MBC3					 = 0x11,
		MBC3_RAM				 = 0x12,
		MBC3_RAM_BATTERY		 = 0x13,
		MBC4					 = 0x15,
		MBC4_RAM				 = 0x16,
		MBC4_RAM_BATTERY		 = 0x17,
		MBC5					 = 0x19,
		MBC5_RAM				 = 0x1A,
		MBC5_RAM_BATTERY		 = 0x1B,
		MBC5_RUMBLE				 = 0x1C,
		MBC5_RUMBLE_RAM			 = 0x1D,
		MBC5_RUMBLE_RAM_BATTERY	 = 0x1E,
		POCKET_CAMERA			 = 0xFC,
		BANDAI_TAMA5			 = 0xFD,
		HuC3					 = 0xFE,
		HuC1_RAM_BATTERY		 = 0xFF
	};
	
	enum CGBFlag : ubyte_t
	{
		No			= 0x00, // DMG Game
		Partial		= 0x80,	// DMG Game with CGB function
		Only		= 0xC0	// CGB only game
	};
	
	Cartridge() =default;
	Cartridge(const std::string& path);
	~Cartridge();

	bool load(const std::string& path);

	byte_t read(addr_t addr) const;
	void write_ram(addr_t addr, byte_t value);
	void write(addr_t addr, byte_t value);

	inline std::string getName() const;
	inline Type getType() const;
	inline bool isMBC1() const;
	inline bool isMBC2() const;
	inline bool isMBC3() const;
	inline bool isMBC5() const;
	inline bool hasBattery() const;
	inline size_t getROMSize() const;
	inline size_t getRAMSize() const;
	inline CGBFlag getCGBFlag() const;
	inline checksum_t getChecksum() const;
	inline unsigned int getHeaderChecksum() const;

	/**
	 * Saves RAM to a file if a battery is present
	**/
	void save() const;

	std::string save_path() const;

private:
	std::vector<byte_t> _data;
	std::vector<byte_t> _ram;

	size_t		_rom_bank = 1;
	size_t		_ram_bank = 0;
	bool		_enable_ram = false;
	size_t		_ram_size = 0;
	byte_t		_mode = 0;

	byte_t		_rtc_registers[5];

	void latch_clock_data();
	static bool file_exists(const std::string& path);
};

#include "Cartridge.inl"

