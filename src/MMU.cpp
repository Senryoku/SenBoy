#include "MMU.hpp"

MMU::MMU() :
	_mem(new word_t[MemSize])
{
}

MMU::~MMU()
{
	delete[] _mem;
}