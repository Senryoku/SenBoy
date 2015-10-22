#pragma once

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
