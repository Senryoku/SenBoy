#pragma once

#include <cassert>
#include <cstring>
#include <iostream>
#include <functional>

#include "Cartridge.hpp"

class MMU
{
public:	
	static constexpr size_t		MemSize = 0x10000; // Bytes
	static constexpr size_t		WRAMSize = 0x1000; // Bytes
	static constexpr size_t		VRAMSize = 0x2000; // Bytes
	
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
		None = 0x00,
		VBlank = 0x01,				// VBlank
		LCDSTAT = 0x02,				// LCD Stat
		TimerOverflow = 0x04,		// Timer
		TransferComplete = 0x08,	// Serial
		Transition = 0x10,			// Joypad
		All = 0xFF
	};
	
	/// 0 = Pressed/Selected
	enum Joypad : word_t
	{
		RightA = 0x01,
		LeftB = 0x02,
		UpSelect = 0x04,
		DownStart = 0x08,
		Direction = 0x10,
		Button = 0x20
	};
	
	Cartridge*		cartridge = nullptr;
	
	bool			force_dmg = false; ///< Force the execution as a simple GameBoy (DMG)
	bool			force_cgb = false; ///< Force the execution as a Color GameBoy (Priority over force_dmg)
	
	using callback_joy = std::function<bool (void)>;
	callback_joy	callback_joy_up;
	callback_joy	callback_joy_down;
	callback_joy	callback_joy_left;
	callback_joy	callback_joy_right;
	callback_joy	callback_joy_select;
	callback_joy	callback_joy_start;
	callback_joy	callback_joy_b;
	callback_joy	callback_joy_a;
	
	MMU();
	
	~MMU();
	
	void reset();
	
	inline word_t read(addr_t addr) const
	{
		if((addr < 0x0100 || (addr >= 0x200 && addr < 0x08FF)) && read(0xFF50) == 0x00) { // Internal ROM (~BIOS)
			return _mem[addr];
		} else if(addr < 0x8000) { // 2 * 16kB ROM Banks
			return static_cast<word_t>(cartridge->read(addr));
		} else if(cgb_mode() && _mem[VBK] != 0 && addr >= 0x8000 && addr < 0xA000) { // Switchable VRAM
			return _vram_bank1[addr - 0x8000];
		} else if(addr >= 0xA000 && addr < 0xC000) { // External RAM
			return cartridge->read(addr);
		} else if(addr >= 0xC000 && addr < 0xD000 && cgb_mode()) { // CGB Mode - Working RAM Bank 0
			return _wram[0][addr - 0xC000];
		} else if(addr >= 0xD000 && addr < 0xE000 && cgb_mode()) { // CGB Mode - Working RAM
			return _wram[get_wram_bank()][addr - 0xD000];
		} else if(addr >= 0xE000 && addr < 0xFE00) { // Internal RAM mirror
			return _mem[addr - 0x2000];
		} else if(addr == BGPD) { // Background Palette Data
			return read_bg_palette_data();
		} else if(addr == OBPD) { // Sprite Palette Data
			return read_sprite_palette_data();
		} else { // Internal RAM (or unused)
			return _mem[addr];
		}
	}
	
	inline word_t read(Register reg)
	{
		return _mem[reg];
	}
	
	inline word_t& rw(Register reg)
	{
		return _mem[reg];
	}
	
	inline word_t read_vram(word_t bank, addr_t addr)
	{
		if(bank == 0)
			return _mem[addr];
		else if(bank == 1);
			return _vram_bank1[addr - 0x8000];
			
		return 0;
	}
	
	inline addr_t read16(addr_t addr)
	{
		return ((static_cast<addr_t>(read(addr + 1)) << 8) & 0xFF00) | read(addr);
	}
	
	inline void	write(addr_t addr, word_t value)
	{
		if(addr < 0x8000) // Memory Banks management
			cartridge->write(addr, value);
		else if(cgb_mode() && read(VBK) != 0 && addr >= 0x8000 && addr < 0xA000) // Switchable VRAM
			_vram_bank1[addr - 0x8000] = value;
		else if(addr >= 0xA000 && addr < 0xC000) // External RAM
			cartridge->write(addr, value);
		else if(addr >= 0xC000 && addr < 0xD000 && cgb_mode()) // CGB Mode - Working RAM Bank 0
			_wram[0][addr - 0xC000] = value;
		else if(addr >= 0xD000 && addr < 0xE000 && cgb_mode()) // CGB Mode - Switchable WRAM Banks
			_wram[get_wram_bank()][addr - 0xD000] = value;
		else if(addr == DIV) // DIV reset when written to
			_mem[DIV] = 0;
		else if(addr == DMA) // Initialize DMA transfer
			init_dma(value);
		else if(addr == HDMA5) // Initialize Vram DMA transfer
			init_vram_dma(value);
		else if(addr == BGPD) // Background Palette Data
			write_bg_palette_data(value);
		else if(addr == OBPD) // Sprite Palette Data
			write_sprite_palette_data(value);
		else if(addr == VBK) // VRAM Memory Bank (This fixes Oracle of Season...)
			_mem[VBK] = value & 1;
		else if(addr == P1) // Joypad Register
			update_joypad(value);
		else if(addr == KEY1) { // Double Speed - Switch
			if(value & 0x01) _mem[KEY1] = (_mem[KEY1] & 0x80) ? 0x00 : 0x80;
		} else
			_mem[addr] = value;
	}
	
	
	inline void	write(addr_t addr, addr_t value)
	{
		write(addr, static_cast<word_t>(value & 0xFF));
		write(addr + 1, static_cast<word_t>(value >> 8));
	}
	
	inline void key_down(word_t type, word_t key)
	{
		rw(IF) |= Transition; // Interrupt
	}
	
	inline void key_up(word_t type, word_t key)
	{
	}
	
	inline bool check_key(word_t type, word_t key) const
	{
		return (read(MMU::P1) & (type | key)) == 0;
	}
	
	inline void update_joypad(word_t value)
	{
		//assert(value == Direction || value == Button);
		_mem[P1] = ~value; //(~value & 0x3F) | 0x0F;// value | 0x0F;
		if(_mem[P1] & Direction)
		{
			if(callback_joy_up()) _mem[P1] &= ~UpSelect;
			if(callback_joy_down()) _mem[P1] &= ~DownStart;
			if(callback_joy_left()) _mem[P1] &= ~LeftB;
			if(callback_joy_right()) _mem[P1] &= ~RightA;
		} else if(_mem[P1] & Button) {
			if(callback_joy_select()) _mem[P1] &= ~UpSelect;
			if(callback_joy_start()) _mem[P1] &= ~DownStart;
			if(callback_joy_b()) _mem[P1] &= ~LeftB;
			if(callback_joy_a()) _mem[P1] &= ~RightA;
		}
	}
	
	inline bool cgb_mode() const 
	{
		/// @todo Better
		return force_cgb || (!force_dmg && cartridge->getCGBFlag() != Cartridge::No);
	}
	
	inline color_t get_bg_color(word_t p, word_t c)
	{
		return get_color(_bg_palette_data, p, c);
	}
	
	inline color_t get_sprite_color(word_t p, word_t c)
	{
		return get_color(_sprite_palette_data, p, c);
	}
	
	inline word_t* getPtr() { return _mem; }
	
	/// CGB Only - Check if a HDMA transfer is pending (should be called once during each HBlank)
	void check_hdma()
	{
		if(_pending_hdma)
		{
			for(addr_t i = 0; i < 0x10; ++i)
				_hdma_dst[i] = read(_hdma_src + i);
			
			_hdma_dst += 0x10;
			_hdma_src += 0x10;
			_hdma_length -= 0x10;
			_mem[HDMA5] = (_hdma_length / 0x10 - 1) & 0xFF;
			if(_hdma_length == 0)
			{
				_pending_hdma = false;
				_mem[HDMA5] = _mem[HDMA5] | 0x80;
			}
		}
	}
	
private:
	word_t*		_mem = nullptr;	///< This represent the whole address space and contains all that doesn't fit elsewhere.
	word_t*		_wram[8];		///< Switchable bank of working RAM (CGB Only)
	word_t*		_vram_bank1;	///< VRAM Bank 1 (Bank 0 is in _mem)
	
	void init_dma(word_t val)
	{
		// Doing it here for now.
		// It couldn't find the exact timing right now.
		addr_t start = val * 0x100;
		for(size_t i = 0; i < 0x100; ++i)
			_mem[0xFE00 + i] = read(start + i);
	}
	
	inline word_t& rw(addr_t addr)
	{
		if(addr < 0x8000) { // 2 * 16kB ROM Banks - Not writable !
			std::cout << "Error: Tried to r/w to 0x" << std::hex << addr << ", which is ROM! Use write instead." << std::endl;
			return _mem[0x0100]; // Dummy value.
		} else if(cgb_mode() && read(VBK) != 0 && addr >= 0x8000 && addr < 0xA000) { // Switchable VRAM
			return _vram_bank1[addr - 0x8000];
		} else if(addr >= 0xC000 && addr < 0xD000 && cgb_mode()) { // CGB Mode - Working RAM Bank 0
			return _wram[0][addr - 0xC000];
		} else if(addr >= 0xD000 && addr < 0xE000 && cgb_mode()) { // CGB Mode - Working RAM
			return _wram[get_wram_bank()][addr - 0xD000];
		} else if(addr >= 0xA000 && addr < 0xC000) { // External RAM
			std::cout << "Error: Tried to r/w to 0x" << std::hex << addr << ", which is ExternalRAM! Use write instead." << std::endl;
			return _mem[0x0100];
		} else if(addr >= 0xE000 && addr < 0xFE00) { // Internal RAM mirror
			return _mem[addr - 0x2000];
		} else { // Internal RAM (or unused)
			return _mem[addr];
		}
	}
	
	// GCB Only
	word_t		_bg_palette_data[8][8];
	word_t		_sprite_palette_data[8][8];
	
	bool		_pending_hdma = false;
	addr_t		_hdma_src = 0;
	word_t* 	_hdma_dst = nullptr;
	addr_t 		_hdma_length = 0;
	
	void init_vram_dma(word_t val)
	{
		/// @todo Proper timing, @see http://gbdev.gg8.se/wiki/articles/Video_Display#Bit7.3D1_-_H-Blank_DMA
		/// @todo 8 cycles by 0x10 bytes transfered?
		
		addr_t src = (read(HDMA2) + (addr_t(read(HDMA1)) << 8)) & 0xFFF0;
		addr_t dst = ((read(HDMA4) + (addr_t(read(HDMA3)) << 8)) & 0x1FF0);
		addr_t length = ((val & 0x7F) + 1) * 0x10;
		assert(dst + length < VRAMSize);
		
		word_t* dest_ptr = ((_mem[VBK]) ? _vram_bank1 : _mem + 0x8000) + dst; 
		if(!(val & 0x80)) // General Purpose DMA
		{
			if(_pending_hdma) // Stop HDMA
			{
				_pending_hdma = false;
				_mem[HDMA5] = _mem[HDMA5] | 0x80;
			}
			
			for(addr_t i = 0; i < length; ++i)
				dest_ptr[i] = read(src + i);
			_mem[HDMA5] = 0xFF;
		} else { // H-Blank DMA
			_hdma_src = src;
			_hdma_dst = dest_ptr;
			_hdma_length = length;
			_pending_hdma = true;
		}
	}
	
	inline size_t get_wram_bank() const
	{
		size_t s = read(SVBK) & 0x7;	// 0 - 7
		s = (s > 0 ? s : 1);			// 1 - 7
		assert(s >= 1 && s < 8);
		return s;
	}
	
	inline word_t read_bg_palette_data() const
	{
		word_t bgpi = read(BGPI);
		word_t c = bgpi & 7;
		word_t p = (bgpi >> 3) & 7;
		return _bg_palette_data[p][c];
	}
	
	inline void write_bg_palette_data(word_t val)
	{
		word_t bgpi = read(BGPI);
		word_t c = bgpi & 7;
		word_t p = (bgpi >> 3) & 7;
		_bg_palette_data[p][c] = val;
		if(bgpi & 0x80) // Auto Increment
			write(BGPI, word_t(0x80 + ((bgpi + 1) & 0x7F)));
	}
	
	inline word_t read_sprite_palette_data() const
	{
		word_t obpi = read(OBPI);
		word_t c = obpi & 7;
		word_t p = (obpi >> 3) & 7;
		return _sprite_palette_data[p][c];
	}
	
	inline void write_sprite_palette_data(word_t val)
	{
		word_t obpi = read(OBPI);
		word_t c = obpi & 7;
		word_t p = (obpi >> 3) & 7;
		_sprite_palette_data[p][c] = val;
		if(obpi & 0x80) // Auto Increment
			write(OBPI, word_t(0x80 + ((obpi + 1) & 0x7F)));
	}
	
	inline color_t get_color(const word_t (&pd)[8][8], word_t p, word_t c)
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
};
