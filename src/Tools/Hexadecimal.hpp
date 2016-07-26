#pragma once

template<typename T>
struct HexaGen
{
	T		v;
	
	explicit HexaGen(T _t) : v(_t) {}
	
	inline std::string str() const
	{
		std::stringstream ss;
		ss << *this;
		return ss.str();
	}
	
	inline operator std::string() const
	{
		return str();
	}
};

template<typename T>
inline std::ostream& operator<<(std::ostream& os, const HexaGen<T>& t)
{
	std::iostream::fmtflags f(os.flags());
	os << "0x" << std::hex << std::uppercase << std::setw(sizeof(T) * 2) << std::setfill('0') << (int) t.v;
	os.flags(f);
	return os;
}

using Hexa = HexaGen<addr_t>;
using Hexa8 = HexaGen<word_t>;
