#pragma once

#include <cassert>
#include <iostream>

#include "ROM.hpp"

class MMU
{
public:
	using word_t = uint8_t;
	using addr_t = uint16_t;
	
	static constexpr size_t			MemSize = 0x10000; // Bytes
	
	enum Register : addr_t
	{
		//
		P1		= 0xFF00, // Joypad
		SB		= 0xFF01, // Serial transfer data
		SC		= 0xFF02, // SIO control
		DIV		= 0xFF04, // Divider Register
		TIMA	= 0xFF05, // Timer counter
		TMA		= 0xFF06, // Timer Modulo
		TAC		= 0xFF07, // Timer Control
		
		// Sound
		NR10	= 0xFF10, // Sound Mode 1 register
		NR11	= 0xFF11, // ...
		NR12	= 0xFF12, // ...
		NR13	= 0xFF13, // ...
		NR14	= 0xFF14, // ...
		NR21	= 0xFF16, // ...
		// And others...
		
		// Video Registers
		LCDC	= 0xFF40,
		STAT	= 0xFF41,
		SCY		= 0xFF42,
		SCX		= 0xFF43,
		LY		= 0xFF44,
		LYC		= 0xFF45,
		BGP		= 0xFF47,
		OBP0	= 0xFF48,
		OBP1	= 0xFF49,
		WY		= 0xFF4A,
		WX		= 0xFF4B,
		
		DMA		= 0xFF46, // DMA Transfer and Start Address
		
		//
		IF		= 0xFF0F, // Interrupt Flag
		IE		= 0xFFFF // Interrupt Enable
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
	
	ROM*		rom = nullptr;
	
	MMU();
	
	~MMU();
	
	inline word_t read(addr_t addr) const
	{
		if(addr < 0x0100 && read(0xFF50) != 0x01) // Internal ROM
			return _mem[addr];
		else if(addr < 0x8000) // 2 * 16kB ROM Banks
			return reinterpret_cast<word_t&>(rom->read(addr));
		else
			return _mem[addr];
	}
	
	inline word_t& rw(addr_t addr)
	{
		if(addr < 0x0100 && read(0xFF50) != 0x01) // Internal ROM
			return _mem[addr];
		else if(addr < 0x8000) // 2 * 16kB ROM Banks
			return reinterpret_cast<word_t&>(rom->read(addr));
		else
			return _mem[addr];
	}
	
	inline word_t& read(Register reg)
	{
		return rw(static_cast<addr_t>(reg));
	}
	
	inline word_t& rw(Register reg)
	{
		return rw(static_cast<addr_t>(reg));
	}
	
	inline addr_t read16(addr_t addr)
	{
		return (static_cast<addr_t>(read(addr + 1)) << 8) + read(addr);
	}
	
	inline void	write(addr_t addr, word_t value)
	{
		rw(addr) = value;
	}
	
	inline void	write(addr_t addr, addr_t value)
	{
		write(addr, static_cast<word_t>(value & 0xFF));
		write(addr + 1, static_cast<word_t>(value >> 8));
	}
	
	inline word_t* getPtr() { return _mem; }
	
private:
	word_t*		_mem = nullptr;
};
