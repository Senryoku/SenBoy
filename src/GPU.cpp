#include "GPU.hpp"

void GPU::reset()
{
	std::memset(screen, 0xFF, ScreenWidth * ScreenHeight * sizeof(color_t));
	
	getLine() = getScrollX() = getScrollY() = getBGPalette() = getLCDControl() = getLCDStatus() = _cycles = 0;
	getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::OAM;
}

void GPU::update_mode()
{
	/// @todo Check if LCD is enabled (and reset LY if it isn't)
	
	switch(getLCDStatus() & LCDMode)
	{
		case Mode::HBlank:
			if(_cycles >= 204)
			{
				_cycles -= 204;
				getLine()++;
				if(getLine() >= ScreenHeight)
				{
					getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::VBlank;
					// VBlank Interrupt
					mmu->rw(MMU::IF) |= MMU::VBlank;
					exec_stat_interrupt(Mode01);
					_completed_frame = true;
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
					getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::OAM;
					exec_stat_interrupt(Mode10);
				}
			}
			break;
		case Mode::OAM:
			if(_cycles >= 80)
			{
				_cycles -= 80;
				getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::VRAM;
			}
			break;
		case Mode::VRAM:
			if(_cycles >= 172)
			{
				_cycles -= 172;
				render_line();
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

	
void GPU::render_line()
{
	word_t line = getLine();
	assert(line < ScreenHeight);
	// Render Background
	if(getLCDControl() & BGWindowDisplay)
	{
		// Selects the Tile Map & Tile Data Set
		addr_t mapoffs = (getLCDControl() & BGTileMapDisplaySelect) ? 0x9C00 : 0x9800;
		addr_t base_tile_data = (getLCDControl() & BGWindowsTileDataSelect) ? 0x8000 : 0x9000;
		
		mapoffs += 0x20 * (((line + getScrollY()) & 0xFF) >> 3);
		word_t lineoffs = (getScrollX() >> 3);

		word_t x = getScrollX() & 0b111;
		word_t y = (getScrollY() + line) & 0b111;
		
		// Tile Index
		word_t tile;
		word_t tile_l;
		word_t tile_h;
		word_t tile_data0 = 0, tile_data1 = 0;
		
		for(word_t i = 0; i < ScreenWidth; ++i)
		{
			if(x == 8 || i == 0) // Loading Tile Data (Next Tile)
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
	// @todo Check OBJSize ? (How does that work ?)
	// @todo Test
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