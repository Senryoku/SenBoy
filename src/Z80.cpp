#include "Z80.hpp"

#include <cassert>

Z80::Z80() :
	_mem(new word_t[RAMSize])
{
	reset();
	
	// Alignment checks.
	assert(&_a == &_r[0]);
	assert(&_b == &_r[1]);
	assert(&_l == &_r[6]);
}

Z80::~Z80()
{
	delete _mem;
}

void Z80::reset()
{
	_pc = 0;
	_sp = 0;
	for(int i = 0; i < 7; ++i)
		_r[i] = 0;
	_f = 0;
	
	_mem = {0};
}
