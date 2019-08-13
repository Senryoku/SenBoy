#pragma once

#include <string>
#include <algorithm>
#include <functional>
#include <fstream>
#include <vector>
#include <cassert>

#include <Tools/Common.hpp>

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
	
	enum HeaderField
	{
		Title			= 0x0134,
		Manufacturer	= 0x013F,
		HCGBFlag		= 0x0143,
		NewLicensee		= 0x0144,
		SGBFlag			= 0x0146,
		CartType		= 0x0147,
		ROMSize			= 0x0148,
		RAMSize			= 0x0149,
		Destination		= 0x014A,
		OldLicensee		= 0x014B,
		Version			= 0x014C,
		HeaderChecksum	= 0x014D,
		Checksum		= 0x014E
	};
	
	enum CGBFlag : ubyte_t
	{
		No			= 0x00, // DMG Game
		Partial		= 0x80,	// DMG Game with CGB function
		Only		= 0xC0	// CGB only game
	};
	
	using LogFunc = std::function<void(const std::string&)>;
	LogFunc Log = LogFunc{};
	
	Cartridge() =default;
	explicit Cartridge(const std::string& path);
	explicit Cartridge(const Cartridge&) =default;
	Cartridge& operator=(const Cartridge& rhs) =default;
	~Cartridge() =default;

	bool load(const std::string& path);
	bool load_from_memory(const unsigned char data[], size_t size);
	void reset();

	byte_t read(addr_t addr) const;
	void write_ram(addr_t addr, byte_t value);
	void write(addr_t addr, byte_t value);

	inline std::string getName() const;				///< @return Game name
	inline Type getType() const;					///< @return Cartridge type (mapper)
	inline bool isMBC1() const;
	inline bool isMBC2() const;
	inline bool isMBC3() const;
	inline bool isMBC5() const;
	inline bool hasBattery() const;
	inline size_t getROMSize() const;
	inline size_t getROMBankCount() const;
	inline bool hasRAM() const;
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
	byte_t		_mode = 0;				///< 0: ROM Banking Mode, 1: RAM Banking Mode

	byte_t		_rtc_registers[5];

	void latch_clock_data();
	static bool file_exists(const std::string& path);
	
	inline int rom_bank() const;
	inline int ram_bank() const;
	
	bool init();
};

#include "Cartridge.inl"

