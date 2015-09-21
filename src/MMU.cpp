#include "MMU.hpp"

#include <cstring>

MMU::MMU() :
	_mem(new word_t[MemSize])
{
	std::memset(_mem, 0x00, MemSize);
}

MMU::~MMU()
{
	delete[] _mem;
}