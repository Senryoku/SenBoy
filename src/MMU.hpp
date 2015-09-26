#pragma once

#include <cassert>
#include <cstring>
#include <iostream>
#include <functional>

#include "Cartridge.hpp"

class MMU
{
public:
	using word_t = uint8_t;
	using addr_t = uint16_t;
	
	static constexpr size_t			MemSize = 0x10000; // Bytes
	
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
		
		DMA		= 0xFF46, ///< DMA Transfer and Start Address
		
		//
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
		if(addr < 0x0100 && read(0xFF50) != 0x01) { // Internal ROM
			return _mem[addr];
		} else if(addr < 0x8000) { // 2 * 16kB ROM Banks
			return static_cast<word_t>(cartridge->read(addr));
		} else if(addr >= 0xA000 && addr < 0xC000) { // External RAM
			return cartridge->read(addr);
		} else if(addr >= 0xE000 && addr < 0xFE00) { // Internal RAM mirror
			return _mem[addr - 0x2000];
		} else { // Internal RAM (or unused)
			return _mem[addr];
		}
	}
	
	inline word_t read(Register reg)
	{
		return read(static_cast<addr_t>(reg));
	}
	
	inline word_t& rw(Register reg)
	{
		return rw(static_cast<addr_t>(reg));
	}
	
	inline addr_t read16(addr_t addr)
	{
		return ((static_cast<addr_t>(read(addr + 1)) << 8) & 0xFF00) | read(addr);
	}
	
	inline void	write(addr_t addr, word_t value)
	{
		if(addr < 0x0100 && read(0xFF50) != 0x01) // Internal ROM
			rw(addr) = value;
		else if(addr < 0x8000) // Memory Banks management
			cartridge->write(addr, value);
		else if(addr >= 0xA000 && addr < 0xC000) // External RAM
			cartridge->write(addr, value);
		else if(addr == DIV) // DIV reset when written to
			_mem[DIV] = 0;
		else if(addr == DMA) // Initialize DMA transfer
			init_dma(value);
		else if(addr == P1) // Joypad Register
			update_joypad(value);
		else
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
	
	inline word_t* getPtr() { return _mem; }
	
private:
	word_t*		_mem = nullptr;
	
	void init_dma(word_t val)
	{
		// Doing it here for now.
		// It couldn't find the exact timing right now.
		addr_t start = val * 0x100;
		std::memcpy(_mem + 0xFE00, _mem + start, 40 * 4);
		/*
		for(addr_t i = 0; i < 40 * 4; ++i)
		{
			write(0xFE00 + i, read(start + i));
		}
		*/
	}
	
	inline word_t& rw(addr_t addr)
	{
		if(addr < 0x0100 && read(0xFF50) != 0x01) { // Internal ROM
			return _mem[addr];
		} else if(addr < 0x8000) { // 2 * 16kB ROM Banks - Not writable !
			std::cout << "Error: Tried to r/w to 0x" << std::hex << addr << ", which is ROM! Use write instead." << std::endl;
			return _mem[0x0100]; // Dummy value.
		} else if(addr >= 0xA000 && addr < 0xC000) { // External RAM
			std::cout << "Error: Tried to r/w to 0x" << std::hex << addr << ", which is ExternalRAM! Use write instead." << std::endl;
			return reinterpret_cast<word_t&>(cartridge->rw(addr));
		} else if(addr >= 0xE000 && addr < 0xFE00) { // Internal RAM mirror
			return _mem[addr - 0x2000];
		} else { // Internal RAM (or unused)
			return _mem[addr];
		}
	}
};
