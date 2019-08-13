#pragma once

#include <cassert>
#include <cstring>
#include <iostream>
#include <functional>

#include <Core/Cartridge.hpp>
#include <Tools/Color.hpp>

class MMU
{
public:	
	static constexpr size_t MemSize = 0x10000; // Bytes
	static constexpr size_t WRAMSize = 0x1000; // Bytes
	static constexpr size_t VRAMSize = 0x2000; // Bytes
	
	enum Register : addr_t
	{
		//
		P1		= 0xFF00, ///< Joypad
		SB		= 0xFF01, ///< Serial transfer data
		SC		= 0xFF02, ///< SIO control
		DIV		= 0xFF04, ///< Divider Register
		TIMA	= 0xFF05, ///< Timer counter
		TMA		= 0xFF06, ///< Timer Modulo
		TAC		= 0xFF07, ///< Timer Control
		
		// Sound
		NR10	= 0xFF10, ///< Sound Mode 1 register
		NR11	= 0xFF11, ///< ...
		NR12	= 0xFF12, ///< ...
		NR13	= 0xFF13, ///< ...
		NR14	= 0xFF14, ///< ...
		NR21	= 0xFF16, ///< ...
		// And others...
		
		// Video Registers
		LCDC	= 0xFF40,
		STAT	= 0xFF41,
		SCY		= 0xFF42,
		SCX		= 0xFF43,
		LY		= 0xFF44,
		LYC		= 0xFF45,
		BGP		= 0xFF47,
		OBP0	= 0xFF48, ///< Object Palette 0
		OBP1	= 0xFF49, ///< Object Palette 1
		WY		= 0xFF4A,
		WX		= 0xFF4B,
		VBK		= 0xFF4F, ///< CGB Mode Only - VRAM Bank
		
		DMA		= 0xFF46, ///< DMA Transfer and Start Address
		
		KEY1	= 0xFF4D, ///< CGB Mode Only - Prepare Speed Switch
		HDMA1	= 0xFF51, ///< CGB Mode Only - New DMA Source, High
		HDMA2	= 0xFF52, ///< CGB Mode Only - New DMA Source, Low
		HDMA3	= 0xFF53, ///< CGB Mode Only - New DMA Destination, High
		HDMA4	= 0xFF54, ///< CGB Mode Only - New DMA Destination, Low
		HDMA5	= 0xFF55, ///< CGB Mode Only - New DMA Length/Mode/Start
		RP 		= 0xFF56, ///< CGB Mode Only - Infrared Communications Port
		BGPI	= 0xFF68, ///< CGB Mode Only - Background Palette Index
		BGPD	= 0xFF69, ///< CGB Mode Only - Background Palette Data
		OBPI	= 0xFF6A, ///< CGB Mode Only - Sprite Palette Index
		OBPD	= 0xFF6B, ///< CGB Mode Only - Sprite Palette Data
		SVBK	= 0xFF70, ///< CGB Mode Only - WRAM Bank
		
		IF		= 0xFF0F, ///< Interrupt Flag
		IE		= 0xFFFF  ///< Interrupt Enable
	};
	
	enum InterruptFlag : word_t
	{
		None				= 0x00,
		VBlank				= 0x01,	// VBlank
		LCDSTAT				= 0x02,	// LCD Stat
		TimerOverflow		= 0x04,	// Timer
		TransferComplete	= 0x08,	// Serial
		Transition			= 0x10,	// Joypad (TODO: Emulate these interrupts? (rarely used))
		All					= 0xFF
	};
	
	/// 0 = Pressed/Selected
	enum Joypad : word_t
	{
		RightA		= 0x01,
		LeftB		= 0x02,
		UpSelect	= 0x04,
		DownStart	= 0x08,
		Button		= 0x10,
		Direction	= 0x20
	};
	
	bool force_dmg = false; ///< Force the execution as a simple GameBoy (DMG)
	bool force_cgb = false; ///< Force the execution as a Color GameBoy (Priority over force_dmg)
	
	using callback_joy = std::function<bool (void)>;
	callback_joy	callback_joy_up;
	callback_joy	callback_joy_down;
	callback_joy	callback_joy_left;
	callback_joy	callback_joy_right;
	callback_joy	callback_joy_select;
	callback_joy	callback_joy_start;
	callback_joy	callback_joy_b;
	callback_joy	callback_joy_a;
	
	explicit MMU(Cartridge& cartridge);
	explicit MMU(const MMU& mmu);
	MMU& operator=(const MMU& mmu) {
		std::memcpy(_mem, mmu._mem, MemSize * sizeof(word_t));
		std::memcpy(_vram_bank1, mmu._vram_bank1, VRAMSize * sizeof(word_t));
		for(int i = 0; i < 8; ++i)
			std::memcpy(_wram[i], mmu._wram[i], WRAMSize * sizeof(word_t));
		
		for(int i = 0; i < 8; ++i)
			for(int j = 0; j < 8; ++j) {
				_bg_palette_data[i][j] = mmu._bg_palette_data[i][j];
				_sprite_palette_data[i][j] = mmu._sprite_palette_data[i][j];
			}
	
		_hdma_cycles = mmu._hdma_cycles;
		_pending_hdma = mmu._pending_hdma;
		_hdma_src = mmu._hdma_src;
		_hdma_dst = mmu._hdma_dst;
		
		return *this;
	}
	~MMU();
	
	void reset();
	
	inline word_t read(addr_t addr) const;
	inline word_t read(Register reg) const;
	inline word_t& rw_reg(Register reg);
	inline word_t read_vram(word_t bank, addr_t addr);
	inline addr_t read16(addr_t addr);
	
	inline void	write(addr_t addr, word_t value);
	inline void	write16(addr_t addr, addr_t value);
	
	inline bool cgb_mode() const;
	
	inline color_t get_bg_color(word_t p, word_t c) { return get_color(_bg_palette_data, p, c); }
	inline color_t get_sprite_color(word_t p, word_t c) { return get_color(_sprite_palette_data, p, c); }
	
	/// Loads a boot room according to gameboy type
	void load_boot();
	/// Loads a boot room from a file
	bool load_boot(const std::string& path);
	/// Loads a custom boot room
	void load_senboot();
	
	inline void key_down(word_t type, word_t key) { rw_reg(IF) |= Transition; }
	
	/// CGB Only - Check if a HDMA transfer is pending (should be called once during each HBlank)
	void check_hdma();
	inline bool hdma_cycles() { bool r = _hdma_cycles; _hdma_cycles = false; return r; }
	
private:
	Cartridge* const _cartridge = nullptr;
	
	word_t*		_mem = nullptr;	///< This represent the whole address space and contains all that doesn't fit elsewhere.
	word_t*		_wram[8];		///< Switchable bank of working RAM (CGB Only)
	word_t*		_vram_bank1;	///< VRAM Bank 1 (Bank 0 is in _mem)
	
	void init_dma(word_t val);
	void update_joypad(word_t value);
	
	// GCB Only
	word_t		_bg_palette_data[8][8];
	word_t		_sprite_palette_data[8][8];
	
	bool		_hdma_cycles = false;	///< Signals the CPU 0x10 bytes has been transfered via HDMA.
	bool		_pending_hdma = false;
	addr_t		_hdma_src = 0;
	word_t* 	_hdma_dst = nullptr;
	
	void init_vram_dma(word_t val);
	
	inline size_t get_wram_bank() const;
	inline word_t read_bg_palette_data() const;
	inline void write_bg_palette_data(word_t val);
	inline word_t read_sprite_palette_data() const;
	inline void write_sprite_palette_data(word_t val);
	inline color_t get_color(const word_t (&pd)[8][8], word_t p, word_t c);
	
    static const word_t gb_boot[256];
    static const word_t sen_boot[256];	///< Custom DMG Boot ROM
    static const word_t gbc_boot[4096];
    static const word_t senc_boot[4096];	///< Custom GBC Boot ROM
};

inline bool MMU::cgb_mode() const 
{
	/// @todo Better
	return force_cgb || (!force_dmg && _cartridge->getCGBFlag() != Cartridge::No);
}

inline word_t MMU::read(addr_t addr) const
{
	switch(addr & 0xF000)
	{
	case 0x0000:
		if((addr < 0x0100 || in_range(addr, 0x200, 0x08FF)) && read(0xFF50) == 0x00) // Internal ROM (~BIOS)
			return _mem[addr];
		[[fallthrough]]; // Other adresses in this range are redirected to the cartridge
	case 0x1000: [[fallthrough]];
	case 0x2000: [[fallthrough]];
	case 0x3000: [[fallthrough]];
	case 0x4000: [[fallthrough]];
	case 0x5000: [[fallthrough]];
	case 0x6000: [[fallthrough]];
	case 0x7000:    									 	                         // 2 * 16kB ROM Banks
		return static_cast<word_t>(_cartridge->read(addr));                          // 0x0000 - 0x8000
	case 0x8000: [[fallthrough]];
	case 0x9000:
		if(cgb_mode() && _mem[VBK] != 0)	                                         // Switchable VRAM
			return _vram_bank1[addr - 0x8000];
		break;
	case 0xA000: [[fallthrough]];
	case 0xB000:								                                     // External RAM
		return _cartridge->read(addr);
	case 0xC000:	
		if(cgb_mode())					                                             // CGB Mode - Working RAM Bank 0
			return _wram[0][addr - 0xC000];
		break;
	case 0xD000:		
		if(cgb_mode())					                                             // CGB Mode - Working RAM
			return _wram[get_wram_bank()][addr - 0xD000];
		break;
	case 0xE000: [[fallthrough]];
	case 0xF000:
		if(addr < 0xFE00)								                             // Internal RAM mirror
			return _mem[addr - 0x2000];
		if(addr == BGPD)													         // Background Palette Data
			return read_bg_palette_data();
		if(addr == OBPD)													         // Sprite Palette Data
			return read_sprite_palette_data();
	}
	
	return _mem[addr];                                                                   // Internal RAM (or unused)
}

inline word_t MMU::read(Register reg) const
{
	return _mem[reg];
}

inline word_t& MMU::rw_reg(Register reg)
{
	return _mem[reg];
}

inline word_t MMU::read_vram(word_t bank, addr_t addr)
{
	if(bank == 0)
		return _mem[addr];
	else if(bank == 1)
		return _vram_bank1[addr - 0x8000];
		
	return 0;
}

inline addr_t MMU::read16(addr_t addr)
{
	return ((static_cast<addr_t>(read(addr + 1)) << 8) & 0xFF00) | read(addr);
}

inline void	MMU::write(addr_t addr, word_t value)
{
	switch(addr & 0xF000)
	{
	case 0x0000: [[fallthrough]];
	case 0x1000: [[fallthrough]];
	case 0x2000: [[fallthrough]];
	case 0x3000: [[fallthrough]];
	case 0x4000: [[fallthrough]];
	case 0x5000: [[fallthrough]];
	case 0x6000: [[fallthrough]];
	case 0x7000: // Memory Banks management (0x0000-0x8000)
		_cartridge->write(addr, value);
		break;
	case 0x8000: [[fallthrough]];
	case 0x9000: // Switchable VRAM
		if(cgb_mode() && read(VBK) != 0) 
			_vram_bank1[addr - 0x8000] = value;
		else 
			_mem[addr] = value;
		break;
	case 0xA000: [[fallthrough]];
	case 0xB000: // External RAM
		_cartridge->write(addr, value);
		break;
	case 0xC000: // CGB Mode - Working RAM Bank 0
		if(cgb_mode()) 
			_wram[0][addr - 0xC000] = value;
		else 
			_mem[addr] = value;
		break;
	case 0xD000: // CGB Mode - Switchable WRAM Banks
		if(cgb_mode()) 
			_wram[get_wram_bank()][addr - 0xD000] = value;
		else 
			_mem[addr] = value;
		break;
	case 0xF000:
		switch(addr)
		{
		case Register::STAT: // Bits 0, 1 & 2 are read only
			// https://www.reddit.com/r/EmuDev/comments/56tqlb/emulating_road_rash_gb/
			// http://www.devrs.com/gb/files/faqs.html#GBBugs
			// In certain situations, writing to the STAT register ($ff41) seems to cause bit 1 of the 
			// IF register ($ff0f) to be set (and thus cause interrupt $48 to occur, if it is enabled). 
			// Due to programming bugs, at least two games (Roadrash, Legend of Zerd) insist on this 
			// quirk, and are incompatible with the GBC.
			// As far as has been figured out, the bug happens everytime ANYTHING (including 00) is 
			// written to the STAT register ($ff41) while the gameboy is either in HBLANK or VBLANK mode.
			// It doesn't seem to happen when the gameboy is in OAM or VRAM mode, or when the display 
			// is disabled. (Info from Martin Korth.)
			if(!cgb_mode() && (_mem[Register::LCDC] & 0x80)
				&& one_of(_mem[Register::STAT] & 3, 0, 1))
				_mem[Register::IF] |= 0b10;
				
			_mem[Register::STAT] = (_mem[Register::STAT] & 7) | (value & 0xF8);
			break;
		case Register::LY: // LY reset when written to
			_mem[Register::LY] = 0;
			break;
		case Register::DIV: // DIV reset when written to
			_mem[Register::DIV] = 0;
			break;
		case Register::DMA: // Initialize DMA transfer
			init_dma(value);
			break;
		case Register::HDMA5: // Initialize Vram DMA transfer
			init_vram_dma(value);
			break;
		case Register::BGPD: // Background Palette Data
			write_bg_palette_data(value);
			break;
		case Register::OBPD: // Sprite Palette Data
			write_sprite_palette_data(value);
			break;
		case Register::VBK: // VRAM Memory Bank (This fixes Oracle of Season...)
			_mem[VBK] = value & 1;
			break;
		case Register::P1: // Joypad Register
			update_joypad(value);
			break;
		case Register::KEY1: // Double Speed - Switch
			if(value & 0x01) _mem[KEY1] = (_mem[KEY1] & 0x80) ? 0x00 : 0x80;
			break;
		default:
			_mem[addr] = value;
			break;
		}
		break;
	default:
		_mem[addr] = value;
	}
}

inline void	MMU::write16(addr_t addr, addr_t value)
{
	write(addr, static_cast<word_t>(value & 0xFF));
	write(addr + 1, static_cast<word_t>(value >> 8));
}

inline size_t MMU::get_wram_bank() const
{
	size_t s = read(SVBK) & 0x7;	// 0 - 7
	s = (s > 0 ? s : 1);			// 1 - 7
	assert(s >= 1 && s < 8);
	return s;
}

inline word_t MMU::read_bg_palette_data() const
{
	word_t bgpi = read(BGPI);
	word_t c = bgpi & 7;
	word_t p = (bgpi >> 3) & 7;
	return _bg_palette_data[p][c];
}

inline void MMU::write_bg_palette_data(word_t val)
{
	word_t bgpi = read(BGPI);
	word_t c = bgpi & 7;
	word_t p = (bgpi >> 3) & 7;
	_bg_palette_data[p][c] = val;
	if(bgpi & 0x80) // Auto Increment
		write(BGPI, word_t(0x80 + ((bgpi + 1) & 0x7F)));
}

inline word_t MMU::read_sprite_palette_data() const
{
	word_t obpi = read(OBPI);
	word_t c = obpi & 7;
	word_t p = (obpi >> 3) & 7;
	return _sprite_palette_data[p][c];
}

inline void MMU::write_sprite_palette_data(word_t val)
{
	word_t obpi = read(OBPI);
	word_t c = obpi & 7;
	word_t p = (obpi >> 3) & 7;
	_sprite_palette_data[p][c] = val;
	if(obpi & 0x80) // Auto Increment
		write(OBPI, word_t(0x80 + ((obpi + 1) & 0x7F)));
}

inline color_t MMU::get_color(const word_t (&pd)[8][8], word_t p, word_t c)
{
	color_t r;
	word_t l = pd[p][c * 2];
	word_t h = pd[p][c * 2 + 1];
	addr_t pal = (h << 8) + l;
	r.r = pal & 0x001F;
	r.g = (pal >> 5) & 0x001F;
	r.b = (pal >> 10) & 0x001F;
	r.a = 255;
	/// @Todo Better response (not linear)
	return r * (256.0 / 32.0);
}
