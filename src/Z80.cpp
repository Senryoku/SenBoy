#include "Z80.hpp"

#include <cstring> // Memset

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
	delete[] _mem;
}

void Z80::reset()
{
	_pc = 0;
	_sp = 0;
	for(int i = 0; i < 7; ++i)
		_r[i] = 0;
	_f = 0;
	
	std::memset(_mem, 0, RAMSize);
}

void Z80::reset_cart()
{
	_pc = 0x0100;
	_sp = 0xFFFE;
	setAF(0x0001);
	_f = 0xB0; // What ?
	setBC(0x0013);
	setDE(0x00D8);
	setHL(0x014D);
	
	write(0xFF05, word_t(0x00));
	write(0xFF06, word_t(0x00));
	write(0xFF07, word_t(0x00));
	write(0xFF10, word_t(0x80));
	write(0xFF11, word_t(0xBF));
	write(0xFF12, word_t(0xF3));
	write(0xFF14, word_t(0xBF));
	write(0xFF16, word_t(0x3F));
	write(0xFF17, word_t(0x00));
	write(0xFF19, word_t(0xBF));
	write(0xFF1A, word_t(0x7F));
	write(0xFF1B, word_t(0xFF));
	write(0xFF1C, word_t(0x9F));
	write(0xFF1E, word_t(0xBF));
	write(0xFF20, word_t(0xFF));
	write(0xFF21, word_t(0x00));
	write(0xFF22, word_t(0x00));
	write(0xFF23, word_t(0xBF));
	write(0xFF24, word_t(0x77));
	write(0xFF25, word_t(0xF3));
	write(0xFF26, word_t(0xF1));
	write(0xFF40, word_t(0x91));
	write(0xFF42, word_t(0x00));
	write(0xFF43, word_t(0x00));
	write(0xFF45, word_t(0x00));
	write(0xFF47, word_t(0xFC));
	write(0xFF48, word_t(0xFF));
	write(0xFF49, word_t(0xFF));
	write(0xFF4A, word_t(0x00));
	write(0xFF4B, word_t(0x00));
	write(0xFFFF, word_t(0x00));
	
	std::memset(_mem, 0, RAMSize);
}

