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
	
	enum OAMOption
	{
		Palette = 0x10,
		XFlip = 0x20,
		YFlip = 0x40,
		Priority = 0x80
	};
	
	GPU()
	{
		screen = new color_t[ScreenWidth * ScreenHeight];
	}
	
	~GPU()
	{
		delete[] screen;
	}
	
	void reset();
	
	inline void step(size_t cycles)
	{
		assert(mmu != nullptr && screen != nullptr);
		_completed_frame = false;
		_cycles += cycles;
		update_mode();
	}
	
	inline bool completed_frame() const { return _completed_frame; } 
	
	inline addr_t to1D(word_t x, word_t y) { return y * ScreenWidth + x; }
	
	inline word_t& getScrollX() const { return mmu->rw(MMU::SCX); }
	inline word_t& getScrollY() const { return mmu->rw(MMU::SCY); }
	inline word_t& getBGPalette() const { return mmu->rw(MMU::BGP); }
	inline word_t& getLCDControl() const { return mmu->rw(MMU::LCDC); }
	inline word_t& getLCDStatus() const { return mmu->rw(MMU::STAT); }
	inline word_t& getLine() const { return mmu->rw(MMU::LY); }
	inline word_t& getLYC() const { return mmu->rw(MMU::LYC); }
		
	/**
	 * Treats bits in l as low bits and in h as high bits of 2bits values.
	**/
	static inline void palette_translation(word_t l, word_t h, word_t& r0, word_t& r1)
	{
		r0 = r1 = 0;
		for(int i = 0; i < 4; i++)
			r0 |= (((l & (1 << (i + 4))) >> (4 + i)) << (2 * i)) | (((h & (1 << (i + 4))) >> (4 + i))  << (2 * i + 1));
		for(int i = 0; i < 4; i++)
			r1 |= ((l & (1 << i)) << (2 * i - i)) | ((h & (1 << i)) << (2 * i + 1 - i));
	}
	
	/// @param val 0 <= val < 4
	inline word_t getPaletteColor(word_t val)
	{
		return Colors[(getBGPalette() >> (val * 2)) & 0b11];
	}
	
private:
	// Timing
	unsigned int	_cycles = 0;
	bool			_completed_frame = false;
	
	inline void exec_stat_interrupt(LCDStatus m)
	{
		if((getLCDStatus() & m))
			mmu->rw(MMU::IF) |= MMU::LCDSTAT;
	}
						
	void update_mode();
		
	void render_line();
};
