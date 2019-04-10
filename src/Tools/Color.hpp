#pragma once

#include <cassert>

#include "Common.hpp"

struct color_t
{
	union
	{
		struct { word_t r, g, b, a; };
		word_t comp[4];
	};
	
	color_t() =default;
	color_t(const color_t&) =default;
	color_t(color_t&&) =default;
	color_t& operator=(const color_t&) =default;
	color_t& operator=(color_t&&) =default;
	
	explicit color_t(word_t w) :
		r{w},
		g{w},
		b{w},
		a{255}
	{
	}
		
	inline bool operator==(const color_t& v)
	{
		return r == v.r && g == v.g && b == v.b && a == v.a;
	}
	
	inline color_t& operator=(word_t val)
	{
		r = g = b = a = val;
		return *this;
	}
	
	inline bool operator==(word_t val) const
	{
		return r == g && g == b && b == a && a == val;
	}
	
	inline word_t& operator[](word_t val)
	{
		assert(val < 4);
		return comp[val];
	}
	
	inline word_t operator[](word_t val) const
	{
		assert(val < 4);
		return comp[val];
	}
};

inline color_t operator+(const color_t& c0, const color_t& c1)
{
	color_t r;
	r.r = std::max(0, std::min(255, c0.r + c1.r));
	r.g = std::max(0, std::min(255, c0.g + c1.g));
	r.b = std::max(0, std::min(255, c0.b + c1.b));
	r.a = std::max(0, std::min(255, c0.a + c1.a));
	return r;
}

inline color_t operator-(const color_t& c0, const color_t& c1)
{
	color_t r;
	r.r = std::max(0, std::min(255, c0.r - c1.r));
	r.g = std::max(0, std::min(255, c0.g - c1.g));
	r.b = std::max(0, std::min(255, c0.b - c1.b));
	r.a = std::max(0, std::min(255, c0.a - c1.a));
	return r;
}
	
inline color_t operator*(float val, const color_t& c)
{
	color_t r;
	r.r = std::max(0.0f, std::min(255.0f, c.r * val));
	r.g = std::max(0.0f, std::min(255.0f, c.g * val));
	r.b = std::max(0.0f, std::min(255.0f, c.b * val));
	r.a = std::max(0.0f, std::min(255.0f, c.a * val));
	return r;
}

inline color_t operator*(const color_t& c, float val)
{
	return val * c;
}
