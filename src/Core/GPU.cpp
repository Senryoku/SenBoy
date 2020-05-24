#include "GPU.hpp"

#include <list>

constexpr word_t GPU::Colors[4];

GPU::GPU(MMU& mmu) :
	_mmu(&mmu),
	_screen(new color_t[ScreenWidth * ScreenHeight])
{
}
	
GPU::GPU(const GPU& gpu) :
	_screen(new color_t[ScreenWidth * ScreenHeight])
{
	*this = gpu;
}

void GPU::reset()
{
	std::memset(_screen.get(), 0xFF, ScreenWidth * ScreenHeight * sizeof(color_t));
	
	get_line() = get_scroll_x() = get_scroll_y() = get_bgp() = get_lcdstat() = _cycles = 0;
	get_lcdc() = 0x91;
	get_lcdstat() = Mode::OAM;
	
	_cycles = 0;
	_completed_frame = false;
}

void GPU::step(size_t cycles, bool render)
{
	assert(_mmu != nullptr && _screen != nullptr);
	static bool s_cleared_screen = false;
	
	_completed_frame = false;
	word_t l = get_line();
	
	if(!enabled())
	{
		if(!s_cleared_screen)
		{
			_cycles = 0;
			get_line() = 0;
			std::memset(_screen.get(), 0xFF, ScreenWidth * ScreenHeight * sizeof(color_t));
			_completed_frame = true;
			s_cleared_screen = true;
		}
		return;
	} else if(s_cleared_screen) {
		_cycles = 0;
		get_line() = 0;
		s_cleared_screen = false;
	}
	
	_cycles += cycles;
	update_mode(render && enabled());
	
	lyc(get_line() != l);
}
	
void GPU::update_mode(bool render)
{	
	switch(get_lcdstat() & LCDMode)
	{
		case Mode::HBlank:
			if(_cycles >= 204)
			{
				_cycles -= 204;
				++get_line();
				if(get_line() < ScreenHeight)
				{
					get_lcdstat() = (get_lcdstat() & ~LCDMode) | Mode::OAM;
					exec_stat_interrupt(Mode10);
				} else {
					get_lcdstat() = (get_lcdstat() & ~LCDMode) | Mode::VBlank;
					exec_stat_interrupt(Mode01);
				}
			}
			break;
		case Mode::VBlank:
		{
			static bool fired_interrupt = false;
			if(!fired_interrupt) { _mmu->rw_reg(MMU::IF) |= MMU::VBlank; fired_interrupt = true; }
			if(_cycles >= 456)
			{
				_cycles -= 456;
				++get_line();
				if(get_line() == 153) {
					get_line() = 0; // 456 cycles at line 0 (instead of 153)
				} else if(get_line() == 1) {
					_completed_frame = true;
					get_line() = 0;
					_window_y = 0;
					get_lcdstat() = (get_lcdstat() & ~LCDMode) | Mode::OAM;
					exec_stat_interrupt(Mode10);
					fired_interrupt = false;
				}
			}
			break;
		}
		case Mode::OAM:
			if(_cycles >= 80)
			{
				_cycles -= 80;
				get_lcdstat() = (get_lcdstat() & ~LCDMode) | Mode::VRAM;
			}
			break;
		case Mode::VRAM:
			if(_cycles >= 172)
			{
				_cycles -= 172;
				if(render) render_line();
				get_lcdstat() = (get_lcdstat() & ~LCDMode) | Mode::HBlank;
				_mmu->check_hdma();
				exec_stat_interrupt(Mode00);
			}
			break;
	}
}

struct Sprite
{
	word_t		idx;
	int			x;
	int			y;
	
	Sprite(word_t _idx = 0, int _x = 0, int _y = 0) :
		idx(_idx), x(_x), y(_y)
	{}
	
	inline bool operator<(const Sprite& s) const
	{
		return x < s.x || idx < s.idx;
	}
};
	
void GPU::render_line()
{
	const word_t line = get_line();
	const word_t LCDC = get_lcdc();
	
	assert(line < ScreenHeight);
	// BG Transparency
	word_t line_color_idx[ScreenWidth];
	
	// CGB Only - Per tile BG priority
	bool line_bg_priorities[ScreenWidth];
	
	int wx = _mmu->read(MMU::WX) - 7; // Can be < 0
	word_t wy = _mmu->read(MMU::WY);
	
	bool draw_window = (LCDC & WindowDisplay) && wx < 160 && line >= wy;

	// BG Disabled, draw blank in non CGB mode
	word_t start_col = 0;
	if(!_mmu->cgb_mode() && !(LCDC & BGDisplay))
	{
		for(word_t i = 0; i < (draw_window ? wx : ScreenWidth); ++i)
		{
			_screen[to1D(i, line)] = 255;
			line_color_idx[i] = 0;
			line_bg_priorities[i] = false;
		}
		start_col = wx; // Skip to window drawing
	}

	// Render Background Or Window
	if((LCDC & BGDisplay) || draw_window)
	{
		// Selects the Tile Map & Tile Data Set
		addr_t mapoffs = (LCDC & BGTileMapDisplaySelect) ? 0x9C00 : 0x9800;
		addr_t base_tile_data = (LCDC & BGWindowsTileDataSelect) ? 0x8000 : 0x9000;
		
		word_t scroll_x = get_scroll_x();
		word_t scroll_y = get_scroll_y();
		
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
		if(!_mmu->cgb_mode())
		{
			for(int i = 0; i < 4; ++i)
				colors_cache[i] = get_bg_color(i);
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
				mapoffs += 0x20 * (_window_y >> 3);
				lineoffs = 0;

				// X & Y in window space.
				x = 8; // Force Tile Fetch
				y = _window_y & 7;
				++_window_y;
				draw_window = false; // No need to do it again.
			}
			
			if(x == 8 || i == start_col) // Loading Tile Data (Next Tile)
			{
				if(_mmu->cgb_mode())
				{
					map_attributes = _mmu->read_vram(1, mapoffs + lineoffs);
					for(int i = 0; i < 4; ++i)
						colors_cache[i] = _mmu->get_bg_color(map_attributes & BackgroundPalette, i);
					vram_bank = (map_attributes & TileVRAMBank) ? 1 : 0;
					xflip = (map_attributes & HorizontalFlip);
					yflip = (map_attributes & VerticalFlip);
				}
				
				x = x & 7;
				tile = _mmu->read_vram(0, mapoffs + lineoffs);
				int idx = tile;
				// If the second Tile Set is used, the tile index is signed.
				if(!(LCDC & BGWindowsTileDataSelect) && (tile & 0x80))
					idx = -((~tile + 1) & 0xFF);
				int Y = yflip ? 7 - y : y;
				tile_l = _mmu->read_vram(vram_bank, base_tile_data + 16 * idx + Y * 2);
				tile_h = _mmu->read_vram(vram_bank, base_tile_data + 16 * idx + Y * 2 + 1);
				palette_translation(tile_l, tile_h, tile_data0, tile_data1);
				lineoffs = (lineoffs + 1) & 31;
			}
			
			word_t color_x = xflip ? (7 - x) : x;
			word_t shift = ((7 - color_x) & 3) << 1;
			word_t color = ((color_x > 3 ? tile_data1 : tile_data0) >> shift) & 0b11;
			line_color_idx[i] = color;
			line_bg_priorities[i] = (map_attributes & BGtoOAMPriority);
			_screen[to1D(i, line)] = colors_cache[color];
			
			++x;
		}
	}
	
	// Render Sprites
	if(get_lcdc() & OBJDisplay)
	{
		word_t Tile, Opt;
		word_t tile_l = 0;
		word_t tile_h = 0;
		word_t tile_data0 = 0, tile_data1 = 0;
		word_t sprite_limit = 10;
		
		// 8*16 Sprites ?
		word_t size = (get_lcdc() & OBJSize) ? 16 : 8;
		
		std::list<Sprite> sprites;
		for(word_t s = 0; s < 40; s++)
		{
			auto y = _mmu->read(0xFE00 + s * 4) - 16;
			 // Visible on this scanline?
			 // (Not testing x: On real hardware, 'out of bounds' sprites still counts towards 
			 // the 10 sprites per scanline limit)
			if(y <= line && (y + size) > line)
				sprites.emplace_back(
					s, 
					_mmu->read(0xFE00 + s * 4 + 1) - 8, 
					y
				);
		}
		
		// If CGB mode, prioriy is only idx, i.e. sprites are already sorted.
		if(!_mmu->cgb_mode())
			sprites.sort();
		
		if(sprites.size() > sprite_limit)
			sprites.resize(sprite_limit);
		
		// Draw the sprites in reverse priority order.
		sprites.reverse();
		
		bool bg_window_no_priority = _mmu->cgb_mode() && !(LCDC & BGDisplay); // (CGB Only: BG loses all priority)
		
		for(const auto& s : sprites)
		{
			// Visible on _screen?
			if(s.x > -8 && s.x < ScreenWidth)
			{
				Tile = _mmu->read(0xFE00 + s.idx * 4 + 2);
				Opt = _mmu->read(0xFE00 + s.idx * 4 + 3);
				if(get_lcdc() & OBJSize) Tile &= 0xFE; // Bit 0 is ignored for 8x16 sprites
				if(s.y - line >= 8 && (Opt & YFlip)) Tile &= 0xFE;
				const word_t palette = _mmu->read((Opt & Palette) ? MMU::OBP1 : MMU::OBP0); // non CGB Only
				// Only Tile Set #0 ?
				int Y = (Opt & YFlip) ? (size - 1) - (line - s.y) : line - s.y;
				const word_t vram_bank = (_mmu->cgb_mode() && (Opt & OBJTileVRAMBank)) ? 1 : 0;
				tile_l = _mmu->read_vram(vram_bank, 0x8000 + 16 * Tile + Y * 2);
				tile_h = _mmu->read_vram(vram_bank, 0x8000 + 16 * Tile + Y * 2 + 1);
				palette_translation(tile_l, tile_h, tile_data0, tile_data1);
				word_t right_limit = (s.x > ScreenWidth - 8 ? ScreenWidth - s.x : 8);
				for(word_t x = (s.x >= 0 ? 0 : -s.x); x < right_limit; x++)
				{
					word_t color_x = (Opt & XFlip) ? x : (7 - x);
					word_t shift = (color_x & 3) << 1;
					word_t color = ((color_x > 3 ? tile_data0 : tile_data1) >> shift) & 3;
					bool over_bg = (!line_bg_priorities[s.x + x] && 					// (CGB Only - BG Attributes)
									!(Opt & Priority)) || line_color_idx[s.x + x] == 0;	// Priority over background or transparency
									
					if(color != 0 && 						// Transparency
						(bg_window_no_priority || over_bg)) // Priority
					{
						_screen[to1D(s.x + x, line)] = _mmu->cgb_mode() ?
							_mmu->get_sprite_color((Opt & PaletteNumber), color) :
							color_t{Colors[(palette >> (color << 1)) & 3]};
					}
				}
			}
		}
	}
}
