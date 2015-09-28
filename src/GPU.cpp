#include "GPU.hpp"

#include <set>

void GPU::reset()
{
	std::memset(screen, 0xFF, ScreenWidth * ScreenHeight * sizeof(color_t));
	
	getLine() = getScrollX() = getScrollY() = getBGPalette() = getLCDControl() = getLCDStatus() = _cycles = 0;
	getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::OAM;
}

void GPU::update_mode(bool render)
{	
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
				if(render) render_line();
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

struct Sprite
{
	GPU::word_t	idx;
	int			x;
	int			y;
	
	Sprite(GPU::word_t _idx, int _x, int _y) :
		idx(_idx), x(_x), y(_y)
	{}
	
	bool operator<(const Sprite& s) const
	{
		/// @todo In GBC mode:
		/// return idx > s.idx;
		
		if(x > s.x)
			return true;
		else
			return idx > s.idx;
	}
};
	
void GPU::render_line()
{
	word_t LCDC = getLCDControl();
	if(!(LCDC & LCDDisplayEnable))
		return;

	word_t line = getLine();
	assert(line < ScreenHeight);
	// Render Background Or Window
	if((LCDC & BGDisplay) || (LCDC & WindowDisplay))
	{
		// Selects the Tile Map & Tile Data Set
		addr_t mapoffs = (LCDC & BGTileMapDisplaySelect) ? 0x9C00 : 0x9800;
		addr_t base_tile_data = (LCDC & BGWindowsTileDataSelect) ? 0x8000 : 0x9000;
		
		word_t scroll_x = getScrollX();
		word_t scroll_y = getScrollY();
		int wx = mmu->read(MMU::WX) - 7; // Can be < 0
		word_t wy = mmu->read(MMU::WY);
		
		bool draw_window = (LCDC & WindowDisplay) && line >= wy;
		
		mapoffs += 0x20 * (((line + scroll_y) & 0xFF) >> 3);
		word_t lineoffs = (scroll_x >> 3);

		word_t x = scroll_x & 0b111;
		word_t y = (scroll_y + line) & 0b111;
		
		// Tile Index
		word_t tile;
		word_t tile_l;
		word_t tile_h;
		word_t tile_data0 = 0, tile_data1 = 0;
		
		word_t colors_cache[4];
		for(int i = 0; i < 4; ++i)
			colors_cache[i] = getBGPaletteColor(i);
		for(word_t i = 0; i < ScreenWidth; ++i)
		{
			// Switch to window rendering
			if(draw_window && i >= wx)
			{
				mapoffs = (LCDC & WindowsTileMapDisplaySelect) ? 0x9C00 : 0x9800;
				mapoffs += 0x20 * (((line + wy) & 0xFF) >> 3);
				lineoffs = (wx >> 3);

				// X & Y in window space.
				x = wx & 0b111;
				y = (wy + line) & 0b111;
				draw_window = false; // No need to do it again.
			}
			
			if(x == 8 || i == 0) // Loading Tile Data (Next Tile)
			{
				x = x & 7;
				tile = mmu->read(mapoffs + lineoffs);
				int idx = tile;
				// If the second Tile Set is used, the tile index is signed.
				if(!(LCDC & BGWindowsTileDataSelect) && (tile & 0x80))
					idx = -((~tile + 1) & 0xFF);
				tile_l = mmu->read(base_tile_data + 16 * idx + y * 2);
				tile_h = mmu->read(base_tile_data + 16 * idx + y * 2 + 1);
				palette_translation(tile_l, tile_h, tile_data0, tile_data1);
				lineoffs = (lineoffs + 1) & 31;
			}
			
			word_t shift = ((7 - x) & 3) * 2;
			GPU::word_t color = ((x > 3 ? tile_data1 : tile_data0) >> shift) & 0b11;
			screen[to1D(i, line)] = colors_cache[color];
			
			++x;
		}
	}
	
	// Render Sprites
	if(getLCDControl() & OBJDisplay)
	{
		word_t Tile, Opt;
		word_t tile_l = 0;
		word_t tile_h = 0;
		word_t tile_data0 = 0, tile_data1 = 0;
		word_t sprite_limit = 10;
		
		// 8*16 Sprites ?
		word_t size = (getLCDControl() & OBJSize) ? 16 : 8;
		
		std::set<Sprite> sprites;
		for(word_t s = 0; s < 40; s++)
		{
			Sprite spr = {s, mmu->read(0xFE00 + s * 4 + 1) - 8, mmu->read(0xFE00 + s * 4) - 16};
			if(spr.y <= line && (spr.y + size) > line)
				sprites.insert(spr);
		}
			
		for(auto& s : sprites)
		{
			// Visible on this scanline ?
			if(s.x > -8 && s.x < ScreenWidth)
			{
				Tile = mmu->read(0xFE00 + s.idx * 4 + 2);
				Opt = mmu->read(0xFE00 + s.idx * 4 + 3);
				if(s.y - line < 8 && getLCDControl() & OBJSize && !(Opt & YFlip)) Tile &= 0xFE;
				if(s.y - line >= 8 && (Opt & YFlip)) Tile &= 0xFE;
				word_t palette = mmu->read((Opt & Palette) ? MMU::OBP1 : MMU::OBP0);
				// Only Tile Set #0 ?
				int Y = (Opt & YFlip) ? (size - 1) - (line - s.y) : line - s.y;
				tile_l = mmu->read(0x8000 + 16 * Tile + Y * 2);
				tile_h = mmu->read(0x8000 + 16 * Tile + Y * 2 + 1);
				palette_translation(tile_l, tile_h, tile_data0, tile_data1);
				for(word_t x = 0; x < 8; x++)
				{
					word_t color_x = (Opt & XFlip) ? x : (7 - x);
					word_t shift = (color_x & 3) * 2;
					GPU::word_t color = ((color_x > 3 ? tile_data0 : tile_data1) >> shift) & 0b11;
					if(s.x + x >= 0 && s.x + x < ScreenWidth && color != 0 &&
						(!(Opt & Priority) || screen[to1D(x, line)] == 0))
					{
						screen[to1D(s.x + x, line)] = Colors[(palette >> (color * 2)) & 0b11];
					}
				}
			}
			if(--sprite_limit == 0) break;
		}
	}
}
