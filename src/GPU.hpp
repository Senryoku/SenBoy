#pragma once

#include <cstring> // Memset

#include "MMU.hpp"

class GPU
{
public:
	using word_t = uint8_t;
	using addr_t = uint16_t;
	struct color_t
	{ 
		word_t r, g, b, a;
		color_t& operator=(word_t val)
		{
			r = g = b = a = val;
			return *this;
		}
	};
	
	MMU*		mmu = nullptr;
	color_t*	screen = nullptr;
	
	const word_t ScreenWidth = 160;
	const word_t ScreenHeight = 144;
	const word_t Colors[4] = {255, 192, 96, 0};
	
	enum Mode : word_t
	{
		HBlank = 0x00,
		VBlank = 0x01,
		OAM = 0x02,
		VRAM = 0x03
	};
	
	enum LCDControl : word_t
	{
		BGWindowDisplay = 0x01,
		OBJDisplay = 0x02,
		OBJSize = 0x04,
		BGTileMapDisplaySelect = 0x08,
		BGWindowsTileDataSelect = 0x10,
		WindowDisplay = 0x2,
		WindowsTileMapDisplaySelect = 0x40,
		LCDControlOperation = 0x80
	};
	
	enum LCDStatus : word_t
	{
		LCDMode = 0b00000011,
		Coincidence = 0b00000100,
		Mode00 = 0b00001000,
		Mode01 = 0b00010000,
		Mode10 = 0b00100000,
		LYC = 0b01000000,
		InterruptSelection = 0b01001000
	};
	
	GPU()
	{
		screen = new color_t[ScreenWidth * ScreenHeight];
		_vram = new word_t[0x1FFF];
		_oam = new word_t[0x00A0];
	}
	
	~GPU()
	{
		delete[] screen;
		delete[] _vram;
		delete[] _oam;
	}
	
	void reset();
	
	void step(size_t cycles)
	{
		_cycles += cycles;
		update_mode();
	}
	
	inline word_t& rw(addr_t addr)
	{
		return mmu->rw(addr);
	}
	
	void write(addr_t addr, word_t val)
	{
		rw(addr) = val;
	}
	
	addr_t to1D(word_t x, word_t y) { return y * ScreenWidth + x; }
	
	word_t& getScrollX() const { return mmu->rw(MMU::SCX); }
	word_t& getScrollY() const { return mmu->rw(MMU::SCY); }
	word_t& getBGPalette() const { return mmu->rw(MMU::BGP); }
	word_t& getLCDControl() const { return mmu->rw(MMU::LCDC);; }
	word_t& getLCDStatus() const { return mmu->rw(MMU::STAT);; }
	word_t& getLine() const { return mmu->rw(MMU::LY);; }
	
private:	
	word_t*			_vram = nullptr;
	word_t*			_oam = nullptr;
	
	// Timing
	unsigned int	_cycles = 0;
	
	void update_mode()
	{
		switch(getLCDStatus() & LCDMode)
		{
			case Mode::HBlank:
				if(_cycles >= 51)
				{
					_cycles -= 51;
					getLine()++;
					if(getLine() >= 143)
					{
						getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::VBlank;
					} else {
						getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::OAM;
					}
				}
				break;
			case Mode::VBlank:
				if(_cycles >= 114)
				{
					_cycles -= 114;
					getLine()++;
					if(getLine() >= 153)
					{
						getLine() = 0;
						getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::HBlank;
					}
				}
				break;
			case Mode::OAM:
				if(_cycles >= 20)
				{
					_cycles -= 20;
					getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::VRAM;
				}
				break;
			case Mode::VRAM:
				if(_cycles >= 43)
				{
					_cycles -= 43;
					renderget_line();
					getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::HBlank;
				}
				break;
		}
	}
	
		
	void renderget_line()
	{
		/// @see http://imrannazar.com/GameBoy-Emulation-in-JavaScript:-Graphics
		addr_t mapoffs = (getLCDControl() & BGTileMapDisplaySelect) ? 0x1C00 : 0x1800;
		mapoffs += ((getLine() + getScrollX()) & 255) >> 3;
		word_t lineoffs = (getScrollX() >> 3);
		
		word_t x = getScrollX() & 7;
		word_t y = (getLine() + getScrollY()) & 7;
		
		word_t tile = rw(mapoffs + lineoffs + 0x8000);
		if((getLCDControl() & BGWindowsTileDataSelect) && tile < 128) tile += 256;
		
		word_t tile_l = _vram[tile * 2];
		word_t tile_h = _vram[tile * 2 + 1];
		word_t tile_data0, tile_data1;
		palette_translation(tile_l, tile_h, tile_data0, tile_data1);
		
		for(word_t i = 0; i < ScreenWidth; i++)
		{
			word_t idx = y * 8 + x;
			word_t color = ((idx > 3 ? tile_data1 : tile_data0) >> (idx * 2)) & 0b11; // 0-3
			screen[to1D(i, getLine())] = getPaletteColor(color);
			
			++x;
			if(x == 8)
			{
				x = 0;
				lineoffs = (lineoffs + 1) & 31;
				tile = _vram[mapoffs + lineoffs];
				tile_l = _vram[tile * 2];
				tile_h = _vram[tile * 2 + 1];
				palette_translation(tile_l, tile_h, tile_data0, tile_data1);
				if((getLCDControl() & BGWindowsTileDataSelect) && tile < 128) tile += 256;
			}
		}
	}
	
	/**
	 * Treats bits in l as low bits and in h as high bits of 2bits values.
	**/
	void palette_translation(word_t l, word_t h, word_t& r0, word_t& r1)
	{
		r0 = r1 = 0;
		for(int i = 0; i < 4; i++)
			r0 |= (((l & (1 << (i + 4))) >> (4 + i)) << (2 * i)) | (((h & (1 << (i + 4))) >> (4 + i))  << (2 * i + 1));
		for(int i = 0; i < 4; i++)
			r1 |= ((l & (1 << i)) << (2 * i - i)) | ((h & (1 << i)) << (2 * i + 1 - i));
	}
	
	/// @param val 0 <= val < 4
	word_t getPaletteColor(word_t val)
	{
		return Colors[(getBGPalette() >> (val * 2)) & 0b11];
	}
};
