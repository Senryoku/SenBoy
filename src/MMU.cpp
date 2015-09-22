#include "MMU.hpp"

#include <cstring>

MMU::MMU() :
	_mem(new word_t[MemSize])
{
	reset();
}

MMU::~MMU()
{
	delete[] _mem;
}

void MMU::reset()
{
	std::memset(_mem, 0x00, MemSize);
}
