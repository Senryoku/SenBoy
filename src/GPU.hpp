#pragma once

#include <cstring> // Memset

#include "MMU.hpp"

class GPU
{
public:	
	MMU*		mmu = nullptr;
	color_t*	screen = nullptr;
	
	const word_t ScreenWidth = 160;
	const word_t ScreenHeight = 144;
	const word_t Colors[4] = {255, 192, 96, 0};
	
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
		InterruptSelection 	= 0b01001000
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
	
	GPU();
	~GPU();
	
	void reset();
	
	inline bool enabled() const
	{
		return getLCDControl() & LCDDisplayEnable;
	}
	
	inline void step(size_t cycles, bool render = true)
	{
		assert(mmu != nullptr && screen != nullptr);
		static bool s_cleared_screen = false;
		
		_completed_frame = false;
		
		if(!enabled())
		{
			getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::VBlank;
			if(!s_cleared_screen)
			{
				std::memset(screen, 0xFF, ScreenWidth * ScreenHeight * sizeof(color_t));
				s_cleared_screen = true;
				_completed_frame = true;
			}
			lyc();
			getLine() = 0;
			return;
		}
		
		s_cleared_screen = false;
		_cycles += cycles;
		update_mode(render);
	}
	
	inline void lyc()
	{
		// Coincidence Bit & Interrupt
		if(getLine() == getLYC())
		{
			getLCDStatus() |= LCDStatus::Coincidence;
			exec_stat_interrupt(LCDStatus::LYC);
		} else {
			getLCDStatus() &= (~LCDStatus::Coincidence);
		}
	}
	
	inline bool completed_frame() const { return _completed_frame; } 
	
	inline addr_t to1D(word_t x, word_t y)
	{
		assert(y < ScreenHeight && x < ScreenWidth);
		return y * ScreenWidth + x;
	}
		
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
	inline word_t getBGPaletteColor(word_t val)
	{
		return Colors[(getBGPalette() >> (val * 2)) & 0b11];
	}
	
	inline word_t& getScrollX() const { return mmu->rw(MMU::Register::SCX); }
	inline word_t& getScrollY() const { return mmu->rw(MMU::Register::SCY); }
	inline word_t& getBGPalette() const { return mmu->rw(MMU::Register::BGP); }
	inline word_t& getLCDControl() const { return mmu->rw(MMU::Register::LCDC); }
	inline word_t& getLCDStatus() const { return mmu->rw(MMU::Register::STAT); }
	inline word_t& getLine() const { return mmu->rw(MMU::Register::LY); }
	inline word_t& getLYC() const { return mmu->rw(MMU::Register::LYC); }
	
private:
	// Timing
	unsigned int	_cycles = 0;
	bool			_completed_frame = false;
	
	inline void exec_stat_interrupt(LCDStatus m)
	{
		if((getLCDStatus() & m))
			mmu->rw(MMU::Register::IF) |= MMU::InterruptFlag::LCDSTAT;
	}

	void update_mode(bool render = true);
		
	void render_line();
};
