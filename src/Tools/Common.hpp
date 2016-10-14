#pragma once

#include <sstream>
#include <iomanip>

using byte_t = char;
using ubyte_t = uint8_t;
using word_t = uint8_t;
using addr_t = uint16_t;
using checksum_t = uint16_t;

#include "Hexadecimal.hpp"

/// return true if v is equal to one of the following args
template<typename T, typename... Args>
inline bool one_of(T&& v, Args&&... args) // Thanks Jason Turner
{
	bool match = false;
	(void) std::initializer_list<bool>{ (match = match || args == v)... };
	return match;
	
	// (In c++17)
	// return ((v == args) || ...);
}

/// return true if v is in [lo, hi[
template<typename T, typename U, typename V>
bool in_range(const T& v, const U& lo, const V& hi)
{
  return v >= lo && v < hi;
}
