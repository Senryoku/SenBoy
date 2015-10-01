#include "MMU.hpp"

#include <cstring>

MMU::MMU() :
	_mem(new word_t[MemSize]),
	_vram_bank1(new word_t[VRAMSize])
{
	for(int i = 0; i < 7; ++i)
		_wram[i] = new word_t[WRAMSize];
	reset();
}

MMU::~MMU()
{
	delete[] _mem;
	delete[] _vram_bank1;
	for(int i = 0; i < 7; ++i)
		delete[] _wram[i];
}

void MMU::reset()
{
	std::memset(_mem, 0x00, MemSize);
	std::memset(_vram_bank1, 0x00, VRAMSize);
	for(int i = 0; i < 7; ++i)
		std::memset(_wram[i], 0x00, WRAMSize);
	for(int i = 0; i < 8; ++i)
		for(int j = 0; j < 8; ++j)
		{
			_bg_palette_data[i][j] = 0xFF;
			_sprite_palette_data[i][j] = 0xFF;
		}
}
