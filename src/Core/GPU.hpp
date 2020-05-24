#pragma once

#include <cstring> // Memset
#include <memory>

#include <Tools/Color.hpp>
#include <Core/MMU.hpp>

class GPU
{
public:
	static constexpr word_t ScreenWidth = 160;
	static constexpr word_t ScreenHeight = 144;
	static constexpr word_t Colors[4] = {255, 192, 96, 0};
	
	enum Mode : word_t
	{
		HBlank 	= 0x00,
		VBlank 	= 0x01,
		OAM 	= 0x02,
		VRAM 	= 0x03
	};
	
	enum LCDControl : word_t
	{
		BGDisplay 					= 0x01,
		OBJDisplay 					= 0x02,
		OBJSize 					= 0x04,
		BGTileMapDisplaySelect 		= 0x08,
		BGWindowsTileDataSelect 	= 0x10,
		WindowDisplay 				= 0x20,
		WindowsTileMapDisplaySelect = 0x40,
		LCDDisplayEnable 			= 0x80
	};
	
	enum LCDStatus : word_t
	{
		LCDMode 			= 0b00000011,
		Coincidence 		= 0b00000100,
		Mode00 				= 0b00001000,
		Mode01 				= 0b00010000,
		Mode10 				= 0b00100000,
		LYC 				= 0b01000000,
		InterruptSelection 	= 0b01111000
	};
	
	enum OAMOption : word_t
	{
		PaletteNumber 	= 0x07,	///< CGB Only
		OBJTileVRAMBank = 0x08,	///< CGB Only
		Palette 		= 0x10,	///< Non CGB Only
		XFlip 			= 0x20,
		YFlip 			= 0x40,
		Priority 		= 0x80
	};
	
	enum BGMapAttribute : word_t
	{
		BackgroundPalette	= 0x07,
		TileVRAMBank		= 0x08,
		HorizontalFlip		= 0x20,
		VerticalFlip		= 0x40,
		BGtoOAMPriority		= 0x80
	};
	
	explicit GPU(MMU& _mmu);
	explicit GPU(const GPU& gpu);
	GPU& operator=(const GPU& gpu) {
		std::memcpy(_screen.get(), gpu._screen.get(), ScreenWidth * ScreenHeight * sizeof(color_t));
		_cycles = gpu._cycles;
		_completed_frame = gpu._completed_frame;
		
		return *this;
	}
	~GPU() =default;
	
	void reset();
	void step(size_t cycles, bool render = true);
	inline bool enabled() const { return get_lcdc() & LCDDisplayEnable; }
	inline bool completed_frame() const { return _completed_frame; } 
	
	inline size_t to1D(word_t x, word_t y);
		
	/**
	 * Treats bits in l as low bits and in h as high bits of 2bits values.
	**/
	static inline void palette_translation(word_t l, word_t h, word_t& r0, word_t& r1);
	
	/// @param val 0 <= val < 4
	inline word_t get_bg_color(word_t val) const { return Colors[(get_bgp() >> (val << 1)) & 3]; }
	
	inline const color_t* get_screen() const { return _screen.get(); }
	inline word_t& get_scroll_x()      const { return _mmu->rw_reg(MMU::Register::SCX); }
	inline word_t& get_scroll_y()      const { return _mmu->rw_reg(MMU::Register::SCY); }
	inline word_t& get_bgp()           const { return _mmu->rw_reg(MMU::Register::BGP); }
	inline word_t& get_lcdc()          const { return _mmu->rw_reg(MMU::Register::LCDC); }
	inline word_t& get_lcdstat()       const { return _mmu->rw_reg(MMU::Register::STAT); }
	inline word_t& get_line()          const { return _mmu->rw_reg(MMU::Register::LY); }
	inline word_t& get_lyc()           const { return _mmu->rw_reg(MMU::Register::LYC); }
	
private:
	MMU* const					_mmu = nullptr;
	std::unique_ptr<color_t[]>	_screen;
	// Timing
	unsigned int				_cycles = 0;
	bool						_completed_frame = false;
	unsigned int 				_window_y = 0; // The window have a distinct line counter (window can be deactivated/reactivated between scanlines)
	
	inline void lyc(bool changed);
	inline void exec_stat_interrupt(LCDStatus m);

	void update_mode(bool render = true);
		
	void render_line();
};

// Inlined member functions

inline void GPU::lyc(bool changed)
{
	// Coincidence Bit & Interrupt
	if(get_line() == get_lyc())
	{
		get_lcdstat() |= LCDStatus::Coincidence;
		if(changed) exec_stat_interrupt(LCDStatus::LYC);
	} else {
		get_lcdstat() &= (~LCDStatus::Coincidence);
	}
}

inline size_t GPU::to1D(word_t x, word_t y)
{
	assert(y < ScreenHeight && x < ScreenWidth);
	return y * ScreenWidth + x;
}

inline void GPU::palette_translation(word_t l, word_t h, word_t& r0, word_t& r1)
{
	r0 = r1 = 0;
	for(int i = 0; i < 4; i++)
		r0 |= (((l & (1 << (i + 4))) >> (4 + i)) << (2 * i)) | (((h & (1 << (i + 4))) >> (4 + i))  << (2 * i + 1));
	for(int i = 0; i < 4; i++)
		r1 |= ((l & (1 << i)) << (2 * i - i)) | ((h & (1 << i)) << (2 * i + 1 - i));
}

inline void GPU::exec_stat_interrupt(LCDStatus m)
{
	if((get_lcdstat() & m))
		_mmu->rw_reg(MMU::Register::IF) |= MMU::InterruptFlag::LCDSTAT;
}
