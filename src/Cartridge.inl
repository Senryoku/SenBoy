/**
 * @file Cartridge inlined function
**/

inline std::string Cartridge::getName() const
{
	if(_data.empty()) return "";
	return std::string(_data.data() + 0x0134, 15);
}

inline Cartridge::Type Cartridge::getType() const
{
	if(_data.empty()) return ROM;
	return Type(*(_data.data() + 0x0147));
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

inline size_t Cartridge::getROMSize() const
{
	size_t s = *(_data.data() + 0x0148);
	if(s < 0x08) return (32 * 1024) << s;
	switch(s)
	{
		case 0x52: return 72 * 0x2000;
		case 0x53: return 80 * 0x2000;
		case 0x54: return 96 * 0x2000;
	}
	// @todo Handle more cases
	return 32 * 1024;
}

inline size_t Cartridge::getRAMSize() const
{
	if(_data.empty()) return 0;
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

inline Cartridge::CGBFlag Cartridge::getCGBFlag() const
{
	byte_t flag = *(_data.data() + 0x0143);
	if(!(flag & 0x80))
		flag = 0;
	return CGBFlag(flag);
}

inline checksum_t Cartridge::getChecksum() const
{
	byte_t h = *(_data.data() + 0x014E);
	byte_t l = *(_data.data() + 0x014F);
	return (h << 8) | l;
}

inline unsigned int Cartridge::getHeaderChecksum() const
{
	return static_cast<unsigned int>(*(_data.data() + 0x014D) & 0xFF);
}
