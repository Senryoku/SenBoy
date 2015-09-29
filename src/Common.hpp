#pragma once

#include <sstream>
#include <iomanip>

using byte_t = char;
using ubyte_t = uint8_t;
using word_t = uint8_t;
using addr_t = uint16_t;
using checksum_t = uint16_t;

template<typename T>
struct HexaGen
{
	T		v;
	HexaGen(T _t) : v(_t) {}
};

template<typename T>
std::ostream& operator<<(std::ostream& os, const HexaGen<T>& t)
{
	return os << "0x" << std::hex << std::setw(sizeof(T) * 2) << std::setfill('0') << (int) t.v;
}

using Hexa = HexaGen<addr_t>;
using Hexa8 = HexaGen<word_t>;