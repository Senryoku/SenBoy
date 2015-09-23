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
	
	void step(size_t cycles)
	{
		_cycles += cycles;
		update_mode();
	}
	
	addr_t to1D(word_t x, word_t y) { return y * ScreenWidth + x; }
	
	word_t& getScrollX() const { return mmu->rw(MMU::SCX); }
	word_t& getScrollY() const { return mmu->rw(MMU::SCY); }
	word_t& getBGPalette() const { return mmu->rw(MMU::BGP); }
	word_t& getLCDControl() const { return mmu->rw(MMU::LCDC);; }
	word_t& getLCDStatus() const { return mmu->rw(MMU::STAT);; }
	word_t& getLine() const { return mmu->rw(MMU::LY);; }
	word_t& getLYC() const { return mmu->rw(MMU::LYC);; }
		
	/**
	 * Treats bits in l as low bits and in h as high bits of 2bits values.
	**/
	static void palette_translation(word_t l, word_t h, word_t& r0, word_t& r1)
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
	
private:
	// Timing
	unsigned int	_cycles = 0;
	
	void exec_stat_interrupt(LCDStatus m)
	{
		if((getLCDStatus() & m))
			mmu->rw(MMU::IF) |= MMU::LCDSTAT;
	}
						
	void update_mode()
	{
		/// @todo Check if LCD is enabled (and reset LY if it isn't)
		
		switch(getLCDStatus() & LCDMode)
		{
			case Mode::HBlank:
				if(_cycles >= 204)
				{
					_cycles -= 204;
					getLine()++;
					if(getLine() >= 143)
					{
						getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::VBlank;
						// VBlank Interrupt
						mmu->rw(MMU::IF) |= MMU::VBlank;
						exec_stat_interrupt(Mode01);
					} else {
						getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::OAM;
						exec_stat_interrupt(Mode10);
					}
				}
				break;
			case Mode::VBlank:
				if(_cycles >= 456)
				{
					_cycles -= 456;
					getLine()++;
					if(getLine() >= 153)
					{
						getLine() = 0; 
						getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::HBlank;
						exec_stat_interrupt(Mode00);
					}
				}
				break;
			case Mode::OAM:
				if(_cycles >= 80)
				{
					_cycles -= 80;
					getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::VRAM;
					exec_stat_interrupt(Mode10); /// @todo Check
				}
				break;
			case Mode::VRAM:
				if(_cycles >= 172)
				{
					_cycles -= 172;
					renderget_line();
					getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::HBlank;
					exec_stat_interrupt(Mode00);
				}
				break;
		}
		
		// Coincidence Bit & Interrupt
		if(getLine() == getLYC())
		{
			getLCDStatus() |= Coincidence;
			exec_stat_interrupt(LYC);
		} else {
			getLCDStatus() = getLCDStatus() & (~Coincidence);
		}
	}
	
		
	void renderget_line()
	{
		auto line = getLine();
		// Render Background
		if(getLCDControl() & BGWindowDisplay)
		{
			// Selects the Tile Map & Tile Data Set
			addr_t mapoffs = (getLCDControl() & BGTileMapDisplaySelect) ? 0x9C00 : 0x9800;
			addr_t base_tile_data = (getLCDControl() & BGWindowsTileDataSelect) ? 0x8000 : 0x9000;
			
			mapoffs += 0x20 * (((line + getScrollY()) & 0xFF) >> 3);
			word_t lineoffs = (getScrollX() >> 3);

			word_t x = getScrollX() & 0b111;
			word_t y = line & 0b111;
			
			// Tile Index
			word_t tile;
			word_t tile_l;
			word_t tile_h;
			word_t tile_data0 = 0, tile_data1 = 0;
			
			for(word_t i = 0; i < ScreenWidth; ++i)
			{
				if(x == 8 || i == 0)
				{
					x = x % 8;
					tile = mmu->read(mapoffs + lineoffs);
					int idx = tile;
					// If the second Tile Set is used, the tile index is signed.
					if(!(getLCDControl() & BGWindowsTileDataSelect) && (tile & 0x80))
						idx = -((~tile + 1) & 0xFF);
					tile_l = mmu->read(base_tile_data + 16 * idx + y * 2);
					tile_h = mmu->read(base_tile_data + 16 * idx + y * 2 + 1);
					palette_translation(tile_l, tile_h, tile_data0, tile_data1);
					lineoffs = (lineoffs + 1) & 31;
				}
				
				word_t shift = ((7 - x) % 4) * 2;
				GPU::word_t color = ((x > 3 ? tile_data1 : tile_data0) >> shift) & 0b11;
				screen[to1D(i, line)] = getPaletteColor(color);
				
				++x;
			}
		}
		
		// Render Sprites
		if(getLCDControl() & OBJDisplay)
		{
			int Y, X;
			word_t Tile, Opt;
			word_t tile_l = 0;
			word_t tile_h = 0;
			word_t tile_data0 = 0, tile_data1 = 0;
			// For each Sprite (4 bytes)
			for(word_t s = 0; s < 40; s++)
			{
				Y = mmu->read(0xFE00 + s * 4) - 16;
				X = mmu->read(0xFE00 + s * 4 + 1) - 8;
				Tile = mmu->read(0xFE00 + s * 4 + 2);
				Opt = mmu->read(0xFE00 + s * 4 + 3);
				// Visible on this scanline ?
				if(Y <= line && (Y + 8) > line)
				{
					word_t palette = mmu->read((Opt & Palette) ? MMU::OBP0 : MMU::OBP1);
					// Only Tile Set #0 ?
					Y = (Opt & YFlip) ? 7 - (line - Y) : line - Y;
					tile_l = mmu->read(0x8000 + 16 * Tile + Y * 2);
					tile_h = mmu->read(0x8000 + 16 * Tile + Y * 2 + 1);
					palette_translation(tile_l, tile_h, tile_data0, tile_data1);
					for(word_t x = 0; x < 8; x++)
					{
						word_t shift = (Opt & XFlip) ? (x % 4) * 2 : ((7 - x) % 4) * 2;
						GPU::word_t color = ((x > 3 ? tile_data1 : tile_data0) >> shift) & 0b11;
						color = Colors[(palette >> (color * 2)) & 0b11];
						if(X + x >= 0 && X + x < ScreenWidth && color != 0 &&
							((Opt | Priority) || screen[to1D(x, line)] == 0))
						{
							screen[to1D(X + x, line)] = color;
						}
					}
				}
			}
		}
	}
};
