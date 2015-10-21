#pragma once

#include <sstream>
#include <iomanip>

using byte_t = char;
using ubyte_t = uint8_t;
using word_t = uint8_t;
using addr_t = uint16_t;
using checksum_t = uint16_t;

struct color_t
{ 
	word_t r, g, b, a;
	
	color_t& operator=(const color_t& v) =default;
	bool operator==(const color_t& v)
	{
		return r == v.r && g == v.g && b == v.b && a == v.a;
	}
	
	color_t& operator=(word_t val)
	{
		r = g = b = a = val;
		return *this;
	}
	
	bool operator==(word_t val)
	{
		return r == g && g == b && b == a && a == val;
	}
	
	color_t operator*(float val) const
	{
		color_t c;
		c.r = std::max(0.0f, std::min(255.0f, r * val));
		c.g = std::max(0.0f, std::min(255.0f, g * val));
		c.b = std::max(0.0f, std::min(255.0f, b * val));
		c.a = std::max(0.0f, std::min(255.0f, a * val));
		return c;
	}
};
	
template<typename T>
struct HexaGen
{
	T		v;
	HexaGen(T _t) : v(_t) {}
	
	std::string str() const
	{
		std::stringstream ss;
		ss << *this;
		return ss.str();
	}
	
	operator std::string() const
	{
		return str();
	}
};

template<typename T>
std::ostream& operator<<(std::ostream& os, const HexaGen<T>& t)
{
	return os << "0x" << std::hex << std::setw(sizeof(T) * 2) << std::setfill('0') << (int) t.v;
}

using Hexa = HexaGen<addr_t>;
using Hexa8 = HexaGen<word_t>;

namespace config
{
	extern std::string _executable_folder;

	// 'Parsing' executable location
	void set_folder(const char* exec_path);
	std::string to_abs(const std::string& path);
}

bool replace(std::string& str, const std::string& from, const std::string& to);
