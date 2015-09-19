#pragma once

#include <cstring> // Memset

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
	}
	
	~GPU()
	{
		delete[] screen;
		delete[] _vram;
	}
	
	void reset()
	{
		std::memset(_vram, 0, 0x1FFF);
		std::memset(screen, 0xFF, ScreenWidth * ScreenHeight * sizeof(color_t));
		
		_line = _scroll_x = _scroll_y = _bkgd_pal = _lcd_control = _lcd_status = _cycles = 0;
		_lcd_status = (_lcd_status & ~LCDMode) | Mode::OAM;
	}
	
	void step()
	{
		update_mode();
		
		
		
		++_cycles;
	}
	
	word_t& read(addr_t addr)
	{
		switch(addr)
		{
			case 0xFF40: return _lcd_control; break;
			case 0xFF41: return _lcd_status; break;
			case 0xFF42: return _scroll_x; break;
			case 0xFF43: return _scroll_y; break;
			case 0xFF44: return _line; break;
			case 0xFF47: return _bkgd_pal; break;
			default: return _vram[addr & 0x1FFF]; break;
		}
	}
	
	void write(addr_t addr, word_t val)
	{
		read(addr) = val;
	}
	
	addr_t to1D(word_t x, word_t y) { return y * ScreenWidth + x; }

private:
	// Registers
	word_t			_scroll_x = 0; ///< Backgound Scroll X Register
	word_t			_scroll_y = 0; ///< Backgound Scroll Y Register
	word_t			_bkgd_pal = 0; ///< Backgound Palette Register
	word_t 			_lcd_control = 0; ///< LCD Control Register
	word_t 			_lcd_status = 0; ///< LCD Status Register
	
	word_t*			_vram = nullptr;
	
	// Timing
	word_t			_line = 0;
	unsigned int	_cycles = 0;
	
	void update_mode()
	{
		switch(_lcd_status & LCDMode)
		{
			case Mode::HBlank:
				if(_cycles >= 51)
				{
					_cycles -= 51;
					_line++;
					if(_line >= 143)
					{
						_lcd_status = (_lcd_status & ~LCDMode) | Mode::VBlank;
					} else {
						_lcd_status = (_lcd_status & ~LCDMode) | Mode::OAM;
					}
				}
				break;
			case Mode::VBlank:
				if(_cycles >= 114)
				{
					_cycles -= 114;
					_line++;
					if(_line >= 153)
					{
						_line = 0;
						_lcd_status = (_lcd_status & ~LCDMode) | Mode::HBlank;
					}
				}
				break;
			case Mode::OAM:
				if(_cycles >= 20)
				{
					_cycles -= 20;
					_lcd_status = (_lcd_status & ~LCDMode) | Mode::VRAM;
				}
				break;
			case Mode::VRAM:
				if(_cycles >= 43)
				{
					_cycles -= 43;
					render_line();
					_lcd_status = (_lcd_status & ~LCDMode) | Mode::HBlank;
				}
				break;
		}
	}
	
		
	void render_line()
	{
		/// @see http://imrannazar.com/GameBoy-Emulation-in-JavaScript:-Graphics
		addr_t mapoffs = (_lcd_control & BGTileMapDisplaySelect) ? 0x1C00 : 0x1800;
		mapoffs += ((_line + _scroll_x) & 255) >> 3;
		word_t lineoffs = (_scroll_x >> 3);
		
		word_t x = _scroll_x & 7;
		word_t y = (_line + _scroll_y) & 7;
		
		word_t tile = read(mapoffs + lineoffs);
		if((_lcd_control & BGWindowsTileDataSelect) && tile < 128) tile += 256;
		
		word_t tile_l = _vram[tile * 2];
		word_t tile_h = _vram[tile * 2 + 1];
		word_t tile_data0, tile_data1;
		palette_translation(tile_l, tile_h, tile_data0, tile_data1);
		
		//std::cout << "Tile: " << (int) tile << " y: " << (int) y << std::endl;
		for(word_t i = 0; i < 160; i++)
		{
			word_t idx = y * 8 + x;
			word_t color = ((idx > 3 ? tile_data1 : tile_data0) >> (idx * 2)) & 0b11; // 0-3
			screen[to1D(i, _line)] = getPaletteColor(color);
			
			++x;
			if(x == 8)
			{
				x = 0;
				lineoffs = (lineoffs + 1) & 31;
				tile = _vram[mapoffs + lineoffs];
				tile_l = _vram[tile * 2];
				tile_h = _vram[tile * 2 + 1];
				palette_translation(tile_l, tile_h, tile_data0, tile_data1);
				if((_lcd_control & BGWindowsTileDataSelect) && tile < 128) tile += 256;
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
		return Colors[(_bkgd_pal >> (val * 2)) & 0b11];
	}
};
