#pragma once

#include <functional>

/**
 * Zilog 80 CPU
 *
 * Gameboy CPU
**/
class Z80
{
public:
	using word_t = uint8_t;
	using addr_t = uint16_t;
	
	static constexpr unsigned int	ClockRate = 4190000; // Hz
	static constexpr size_t			RAMSize = 0xFFFF; // Bytes
	
	enum Flag : word_t
	{
		Zero = 0x80,		///< Last result was 0x00
		Negative = 0x40,	///< Last result was < 0x00
		HalfCarry = 0x20,	///< Last result was > 0x0F
		Carry = 0x10		///< Last result was > 0xFF or < 0x00
	};
	
	Z80();
	~Z80();
	
	/// Reset the CPU, zero-ing all registers.
	void reset();
	
private:
	///////////////////////////////////////////////////////////////////////////
	// Registers
	
	addr_t	_pc;	///< Program Counter
	addr_t	_sp;	///< Stack Pointer Register
	word_t	_f;		///< Flags Register
	
	union ///< 8 bits general purpose registers.
	{
		struct 
		{
			word_t	_a;		///< A Register (Accumulator)
			word_t	_b;		///< B Register (Counter)
			word_t	_c;		///< C Register (Counter)
			word_t	_d;		///< D Register
			word_t	_e;		///< E Register
			word_t	_h;		///< F Register
			word_t	_l;		///< L Register
		};
		word_t _r[7];		///< Another way to access the 8 bits registers.
	};
	
	///////////////////////////////////////////////////////////////////////////
	// Cycles management
	
	unsigned int	_clock_cycles = 0;
	unsigned int	_machine_cycles = 0;
	
	inline void add_cycles(unsigned int c)
	{
		_clock_cycles += c;
		_machine_cycles += 4 * c;
	}
	
	///////////////////////////////////////////////////////////////////////////
	// RAM management
	
	word_t*	_mem;	///< RAM
	inline word_t	read(addr_t addr) const				{ return _mem[addr]; }
	inline void		write(addr_t addr, word_t value)	{ _mem[addr] = value; }
	
	///////////////////////////////////////////////////////////////////////////
	// Instructions
	
	inline void instr_nop() { add_cycles(1); }
	
	enum Instr : word_t
	{
		NOP = 0x00
	};
};
