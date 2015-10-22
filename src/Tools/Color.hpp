#pragma once

#include "Common.hpp"

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
