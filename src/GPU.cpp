#include "GPU.hpp"

#include <list>

GPU::GPU()
{
	screen = new color_t[ScreenWidth * ScreenHeight];
}

GPU::~GPU()
{
	delete[] screen;
}
	
void GPU::reset()
{
	std::memset(screen, 0xFF, ScreenWidth * ScreenHeight * sizeof(color_t));
	
	getLine() = getScrollX() = getScrollY() = getBGPalette() = getLCDControl() = getLCDStatus() = _cycles = 0;
	getLCDStatus() = LCDDisplayEnable | Mode::OAM;
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
				if(getLine() < ScreenHeight)
				{
					getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::OAM;
					exec_stat_interrupt(Mode10);
				} else {
					getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::VBlank;
					// VBlank Interrupt
					mmu->rw(MMU::IF) |= MMU::VBlank;
					exec_stat_interrupt(Mode01);
				}
			}
			break;
		case Mode::VBlank:
			if(_cycles >= 456)
			{
				_cycles -= 456;
				getLine()++;
				if(getLine() == 152) {
					getLine() = 0; // 456 cycles at line 0 (instead of 153)
				} else if(getLine() == 1) {
					_completed_frame = true;
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
				mmu->check_hdma();
				exec_stat_interrupt(Mode00);
			}
			break;
	}
	
	lyc();
}

struct Sprite
{
	word_t		idx;
	int			x;
	int			y;
	
	Sprite(word_t _idx = 0, int _x = 0, int _y = 0) :
		idx(_idx), x(_x), y(_y)
	{}
	
	bool operator<(const Sprite& s) const
	{
		if(x < s.x)
			return true;
		else
			return idx < s.idx;
	}
};
	
void GPU::render_line()
{
	word_t line = getLine();
	word_t LCDC = getLCDControl();
	if(!(LCDC & LCDDisplayEnable))
	{
		for(word_t i = 0; i < ScreenWidth; ++i)
			screen[to1D(i, line)] = 0xFF;
		return;
	}
	
	assert(line < ScreenHeight);
	// BG Transparency
	word_t line_color_idx[ScreenWidth];
	
	// CGB Only
	bool line_bg_priorities[ScreenWidth];
	
	int wx = mmu->read(MMU::WX) - 7; // Can be < 0
	word_t wy = mmu->read(MMU::WY);
	
	bool draw_window = (LCDC & WindowDisplay) && wx < 160 && line >= wy;
		
	// BG Disabled, draw blank in non CGB mode
	word_t start_col = 0;
	if(!mmu->cgb_mode() && !(LCDC & BGDisplay))
	{
		for(word_t i = 0; i < (draw_window ? wx : ScreenWidth); ++i)
		{
			screen[to1D(i, line)] = 255;
			line_color_idx[i] = 0;
		}
		start_col = wx; // Skip to window drawing
	}
		
	// Render Background Or Window
	if((LCDC & BGDisplay) || draw_window)
	{
		// Selects the Tile Map & Tile Data Set
		addr_t mapoffs = (LCDC & BGTileMapDisplaySelect) ? 0x9C00 : 0x9800;
		addr_t base_tile_data = (LCDC & BGWindowsTileDataSelect) ? 0x8000 : 0x9000;
		
		word_t scroll_x = getScrollX();
		word_t scroll_y = getScrollY();
		
		mapoffs += 0x20 * (((line + scroll_y) & 0xFF) >> 3);
		word_t lineoffs = (scroll_x >> 3);

		word_t x = scroll_x & 7;
		word_t y = (scroll_y + line) & 7;
		
		// Tile Index
		word_t tile;
		word_t tile_l;
		word_t tile_h;
		word_t tile_data0 = 0, tile_data1 = 0;
		
		color_t colors_cache[4];
		if(!mmu->cgb_mode())
		{
			for(int i = 0; i < 4; ++i)
				colors_cache[i] = getBGPaletteColor(i);
		}
		
		// CGB Only
		word_t	map_attributes = 0;
		word_t	vram_bank = 0;
		bool	xflip = false;
		bool	yflip = false;
		
		for(word_t i = start_col; i < ScreenWidth; ++i)
		{
			// Switch to window rendering
			if(draw_window && i >= wx)
			{
				mapoffs = (LCDC & WindowsTileMapDisplaySelect) ? 0x9C00 : 0x9800;
				mapoffs += 0x20 * (((line - wy)) >> 3);
				lineoffs = 0;

				// X & Y in window space.
				x = 8; // Force Tile Fetch
				y = (line - wy) & 7;
				draw_window = false; // No need to do it again.
			}
			
			if(x == 8 || i == start_col) // Loading Tile Data (Next Tile)
			{
				if(mmu->cgb_mode())
				{
					map_attributes = mmu->read_vram(1, mapoffs + lineoffs);
					for(int i = 0; i < 4; ++i)
						colors_cache[i] = mmu->get_bg_color(map_attributes & BackgroundPalette, i);
					vram_bank = (map_attributes & TileVRAMBank) ? 1 : 0;
					xflip = (map_attributes & HorizontalFlip);
					yflip = (map_attributes & VerticalFlip);
				}
				
				x = x & 7;
				tile = mmu->read_vram(0, mapoffs + lineoffs);
				int idx = tile;
				// If the second Tile Set is used, the tile index is signed.
				if(!(LCDC & BGWindowsTileDataSelect) && (tile & 0x80))
					idx = -((~tile + 1) & 0xFF);
				int Y = yflip ? 7 - y : y;
				tile_l = mmu->read_vram(vram_bank, base_tile_data + 16 * idx + Y * 2);
				tile_h = mmu->read_vram(vram_bank, base_tile_data + 16 * idx + Y * 2 + 1);
				palette_translation(tile_l, tile_h, tile_data0, tile_data1);
				lineoffs = (lineoffs + 1) & 31;
			}
			
			word_t color_x = xflip ? (7 - x) : x;
			word_t shift = ((7 - color_x) & 3) * 2;
			word_t color = ((color_x > 3 ? tile_data1 : tile_data0) >> shift) & 0b11;
			line_color_idx[i] = color;
			line_bg_priorities[i] = (map_attributes & BGtoOAMPriority);
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
		
		std::list<Sprite> sprites;
		for(word_t s = 0; s < 40; s++)
		{
			Sprite spr = {
				s, 
				mmu->read(0xFE00 + s * 4 + 1) - 8, 
				mmu->read(0xFE00 + s * 4) - 16,
			};
			if(spr.y <= line && (spr.y + size) > line) // Visible on this scanline?
				sprites.push_back(spr);
		}
		
		// If CGB mode, prioriy is only idx, i.e. sprites are already sorted.
		if(!mmu->cgb_mode())
			sprites.sort();
		
		if(sprites.size() > sprite_limit)
			sprites.resize(sprite_limit);
		
		// Draw the sprites in reverse priority order.
		sprites.reverse();
		
		bool bg_window_no_priority = mmu->cgb_mode() && !(LCDC & BGDisplay); // (CGB Only: BG loses all priority)
		
		for(auto& s : sprites)
		{
			// Visible on screen?
			if(s.x > -8 && s.x < ScreenWidth)
			{
				Tile = mmu->read(0xFE00 + s.idx * 4 + 2);
				Opt = mmu->read(0xFE00 + s.idx * 4 + 3);
				if(s.y - line < 8 && getLCDControl() & OBJSize && !(Opt & YFlip)) Tile &= 0xFE;
				if(s.y - line >= 8 && (Opt & YFlip)) Tile &= 0xFE;
				word_t palette = mmu->read((Opt & Palette) ? MMU::OBP1 : MMU::OBP0); // non CGB Only
				// Only Tile Set #0 ?
				int Y = (Opt & YFlip) ? (size - 1) - (line - s.y) : line - s.y;
				word_t vram_bank = (mmu->cgb_mode() && (Opt & OBJTileVRAMBank)) ? 1 : 0;
				tile_l = mmu->read_vram(vram_bank, 0x8000 + 16 * Tile + Y * 2);
				tile_h = mmu->read_vram(vram_bank, 0x8000 + 16 * Tile + Y * 2 + 1);
				palette_translation(tile_l, tile_h, tile_data0, tile_data1);
				for(word_t x = 0; x < 8; x++)
				{
					word_t color_x = (Opt & XFlip) ? x : (7 - x);
					word_t shift = (color_x & 3) * 2;
					word_t color = ((color_x > 3 ? tile_data0 : tile_data1) >> shift) & 0b11;
					
					bool over_bg = (!line_bg_priorities[s.x + x] && 					// (CGB Only - BG Attributes)
									!(Opt & Priority)) || line_color_idx[s.x + x] == 0;	// Priority over background or transparency
									
					if(s.x + x >= 0 && s.x + x < ScreenWidth && 		// On screen
						color != 0 && 									// Transparency
						(bg_window_no_priority || over_bg))
					{
						color_t c;
						if(mmu->cgb_mode())
							c = mmu->get_sprite_color((Opt & PaletteNumber), color);
						else 
							c = Colors[(palette >> (color * 2)) & 0b11];
						screen[to1D(s.x + x, line)] = c;
					}
				}
			}
		}
	}
}
