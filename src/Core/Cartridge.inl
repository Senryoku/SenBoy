/**
 * @file Cartridge inlined function
**/

inline std::string Cartridge::getName() const
{
	if(_data.empty()) return "";
	return std::string(_data.data() + Title, 15);
}

inline Cartridge::Type Cartridge::getType() const
{
	if(_data.empty()) return ROM;
	return Type(*(_data.data() + CartType));
}

inline bool Cartridge::isMBC1() const
{
	return getType() <= MBC1_RAM_BATTERY;
}

inline bool Cartridge::isMBC2() const
{
	return getType() >= MBC2 && getType() <= MBC2_BATTERY;
}

inline bool Cartridge::isMBC3() const
{
	return getType() >= MBC3_TIMER_BATTERY && getType() <= MBC3_RAM_BATTERY;
}

inline bool Cartridge::isMBC5() const
{
	return getType() >= MBC5 && getType() <= MBC5_RUMBLE_RAM_BATTERY;
}

inline bool Cartridge::hasBattery() const
{
	return !_data.empty() && (
		getType() == MBC1_RAM_BATTERY ||
		getType() == MBC2_BATTERY ||
		getType() == ROM_RAM_BATTERY ||
		getType() == MMM01_RAM_BATTERY ||
		getType() == MBC3_TIMER_BATTERY ||
		getType() == MBC3_TIMER_RAM_BATTERY ||
		getType() == MBC3_RAM_BATTERY ||
		getType() == MBC4_RAM_BATTERY ||
		getType() == MBC5_RAM_BATTERY ||
		getType() == MBC5_RUMBLE_RAM_BATTERY ||
		getType() == HuC1_RAM_BATTERY);
}

inline bool Cartridge::hasRAM() const
{
	return !_data.empty() && (
		getType() == MBC1_RAM || 
		getType() == MBC1_RAM_BATTERY || 
		getType() == ROM_RAM || 			
		getType() == ROM_RAM_BATTERY || 
		getType() == MMM01_RAM	 || 
		getType() == MMM01_RAM_BATTERY	 || 
		getType() == MBC3_TIMER_RAM_BATTERY	 || 
		getType() == MBC3_RAM || 
		getType() == MBC3_RAM_BATTERY || 
		getType() == MBC4_RAM || 
		getType() == MBC4_RAM_BATTERY || 
		getType() == MBC5_RAM || 
		getType() == MBC5_RAM_BATTERY || 
		getType() == MBC5_RUMBLE_RAM || 
		getType() == MBC5_RUMBLE_RAM_BATTERY || 
		getType() == HuC1_RAM_BATTERY);
}

inline size_t Cartridge::getROMSize() const
{
	size_t s = *(_data.data() + ROMSize);
	if(s < 0x08) return (32 * 1024) << s;
	switch(s)
	{
		case 0x52: return 72 * 0x4000;
		case 0x53: return 80 * 0x4000;
		case 0x54: return 96 * 0x4000;
	}
	// @todo Handle more cases
	return 32 * 1024;
}

inline size_t Cartridge::getRAMSize() const
{
	if(_data.empty()) return 0;
	switch(*(_data.data() + RAMSize))
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

inline Cartridge::CGBFlag Cartridge::getCGBFlag() const
{
	byte_t flag = *(_data.data() + HCGBFlag);
	if(!(flag & 0x80))
		flag = 0;
	return CGBFlag(flag);
}

inline checksum_t Cartridge::getChecksum() const
{
	byte_t h = *(_data.data() + Checksum);
	byte_t l = *(_data.data() + Checksum + 1);
	return (h << 8) | l;
}

inline unsigned int Cartridge::getHeaderChecksum() const
{
	return static_cast<unsigned int>(*(_data.data() + HeaderChecksum) & 0xFF);
}
	
inline byte_t Cartridge::rom_bank() const
{
	if(isMBC1())
	{
		if(_mode == 0 && *(_data.data() + CartType) >= 0x05)
			return _rom_bank | ((_ram_bank & 3) << 5);
		else
			return _rom_bank;
	} else {
		return _rom_bank;
	}
}

inline byte_t Cartridge::ram_bank() const
{
	if(isMBC1())
	{
		if(_mode == 1)
			return _ram_bank;
		else
			return 0;
	} else {
		return _ram_bank;
	}
}