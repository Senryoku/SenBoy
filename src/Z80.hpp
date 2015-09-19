#pragma once

#include <iostream>

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
		Negative = 0x40,	///< Last operation was a substraction
		HalfCarry = 0x20,	///< Last result was > 0x0F
		Carry = 0x10		///< Last result was > 0xFF or < 0x00
	};
	
	Z80();
	~Z80();
	
	/// Reset the CPU, zero-ing all registers.
	void reset();
	
	void execute()
	{
		word_t opcode = _mem[_pc++];
		// Decode opcode (http://www.z80.info/decoding.htm)
		// Check prefixes
		if(opcode == 0xCB) // Almost done @todo: Impl. S&R
		{
			opcode = _mem[_pc++];
			word_t x = opcode >> 6; // bits 6 & 7
			word_t y = (opcode >> 3) & 0b111; // bits 5 - 3
			word_t reg = extract_src_reg(opcode);;
			
			switch(x)
			{
				case 0b00: // Shift & Rotate - @todo Implem.
				{
					word_t& value = fetchReg(reg);
					
					switch((opcode >> 2) & 0b111)
					{
						case 0: instr_rlc(value); break;
						case 1: instr_rrc(value); break;
						case 2: instr_rl(value); break;
						case 3: instr_rr(value); break;
						case 4: instr_sla(value); break;
						case 5: instr_sra(value); break;
						case 6: instr_swap(value); break;
						case 7: instr_srl(value); break;
					}
					break;
				}
				case 0b01: instr_bit(y, fetchReg(reg)); break;
				case 0b10: instr_res(y, fetchReg(reg)); break;
				case 0b11: instr_set(y, fetchReg(reg)); break;
				default: instr_nop(); break;
			}
		} else {
			word_t x = opcode >> 6; // bits 6 & 7
			//word_t y = (opcode >> 3) & 0b111; // bits 5 - 3
			//word_t z = opcode & 0b111; // bits 2 - 0
			//word_t p = y >> 1; // bits 5 & 4
			//word_t q = y & 1; // bit 3
			
			switch(x)
			{
				case 0b00:
				{
					break;
				}
				case 0b01: // LD on registers - Done ?
				{
					word_t reg_src = extract_src_reg(opcode);
					word_t reg_dst = extract_dst_reg(opcode);
					if(reg_src > 6 && reg_dst > 6) // (HL), (HL) => HALT !
						instr_halt();
					else
						instr_ld(fetchReg(reg_dst), fetchReg(reg_src));
					break;
				}
				case 0b10: // ALU on registers - Done ?
				{ 
					word_t reg_src = extract_src_reg(opcode);
					word_t value = fetchReg(reg_src);
					
					switch((opcode >> 2) & 0b111)
					{
						case 0: instr_add(value); break;
						case 1: instr_adc(value); break;
						case 2: instr_sub(value); break;
						case 3: instr_sbc(value); break;
						case 4: instr_and(value); break;
						case 5: instr_xor(value); break;
						case 6: instr_or(value); break;
						case 7: instr_cp(value); break;
					}
					break;
				}
				case 0b11:
				{
					break;
				}
				default:
					instr_nop();
					break;
			}
		}
	}
	
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
	
	// HL access helpers
	inline addr_t getHL() const { return (static_cast<addr_t>(_h) << 8) + _l; }
	inline word_t& fetchHL() { add_cycles(1); return _mem[getHL()]; }
	inline word_t& fetchReg(word_t n) { return (n > 6) ? fetchHL() : _r[n]; }
	inline void setHL(addr_t val) { _h = (val >> 8) & 0xFF; _l = val & 0xFF; } 
	
	// Flags helpers
	/// Sets (or clears if false is passed as second argument) the specified flag.
	inline void set(Flag m, bool b = true) { _f = b ? (_f | m) : (_f & ~m); }
	/// Return true if the specified flag is set, false otherwise.
	inline bool check(Flag m) { return _f & m; }
	
	///////////////////////////////////////////////////////////////////////////
	// Cycles management
	
	unsigned int	_clock_cycles = 0;
	unsigned int	_machine_cycles = 0;
	
	inline void add_cycles(unsigned int c, unsigned int m = 0)
	{
		_clock_cycles += c;
		_machine_cycles += (m == 0) ? 4 * c : m;
	}
	
	///////////////////////////////////////////////////////////////////////////
	// RAM management
	
	word_t*	_mem;	///< RAM
	inline word_t	read(addr_t addr) const				{ return _mem[addr]; }
	inline void		write(addr_t addr, word_t value)	{ _mem[addr] = value; }
	
	///////////////////////////////////////////////////////////////////////////
	// Instructions
	
	// Helper function on opcodes
	inline word_t extract_src_reg(word_t opcode) const { return (opcode + 1) & 0b111; }
	inline word_t extract_dst_reg(word_t opcode) const { return ((opcode >> 3) + 1) & 0b111; }
	
	inline void instr_nop() { add_cycles(1); }
	
	inline void instr_ld_r_hl(word_t& dst)
	{
		dst = read(getHL());
		add_cycles(2);
	}
	
	inline void instr_ld_hl_r(word_t src)
	{
		setHL(src);
		add_cycles(2);
	}
	
	inline void instr_ld(word_t& dst, word_t src)
	{
		dst = src;
		add_cycles(1);
	}
	
	inline void instr_add(word_t src)
	{
		uint16_t t = _a + src;
		set(Flag::HalfCarry, t > 0x0F);
		set(Flag::Carry, t > 0xFF);
		_a = t & 0xFF;
		set(Flag::Zero, _a == 0);
		set(Flag::Negative, false);
		add_cycles(1);
	}
	
	inline void instr_adc(word_t src)
	{
		uint16_t t = _a + src + (check(Flag::Carry) ? 1 : 0);
		set(Flag::HalfCarry, t > 0x0F);
		set(Flag::Carry, t > 0xFF);
		_a = t & 0xFF;
		set(Flag::Zero, _a == 0);
		set(Flag::Negative, false);
		add_cycles(1);
	}
	
	inline void instr_sub(word_t src)
	{
		int16_t t = _a - src;
		set(Flag::HalfCarry, t > 0x0F);
		set(Flag::Carry, t > 0xFF);
		_a = t & 0xFF;
		set(Flag::Zero, _a == 0);
		set(Flag::Negative, true);
		add_cycles(1);
	}
	
	inline void instr_sbc(word_t src)
	{
		int16_t t = _a - src - (check(Flag::Carry) ? 1 : 0);
		set(Flag::HalfCarry, t > 0x0F);
		set(Flag::Carry, t > 0xFF);
		_a = t & 0xFF;
		set(Flag::Zero, _a == 0);
		set(Flag::Negative, true);
		add_cycles(1);
	}
	
	inline void instr_and(word_t src)
	{
		_a &= src;
		set(Flag::Zero, _a == 0);
		set(Flag::Negative, false);
		set(Flag::HalfCarry, true);
		set(Flag::Carry, false);
		add_cycles(1);
	}
	
	inline void instr_xor(word_t src)
	{
		_a ^= src;
		set(Flag::Zero, _a == 0);
		set(Flag::Negative, false);
		set(Flag::HalfCarry, false);
		set(Flag::Carry, false);
		add_cycles(1);
	}
	
	inline void instr_or(word_t src)
	{
		_a |= src;
		set(Flag::Zero, _a == 0);
		set(Flag::Negative, false);
		set(Flag::HalfCarry, false);
		set(Flag::Carry, false);
		add_cycles(1);
	}
	
	inline void instr_cp(word_t src)
	{
		int16_t t = _a - src;
		set(Flag::HalfCarry, t > 0x0F);
		set(Flag::Carry, t > 0xFF);
		set(Flag::Zero, t == 0);
		set(Flag::Negative, true);
		add_cycles(1);
	}
	
	inline void instr_bit(word_t bit, word_t r)
	{
		set(Flag::Zero, r & (1 << bit));
		set(Flag::Negative, false);
		set(Flag::HalfCarry);
		add_cycles(2);
	}
	
	inline void instr_rlc(word_t v)
	{
		std::cerr << __PRETTY_FUNCTION__ << " not implemented!" << std::endl;
	}
	
	inline void instr_rrc(word_t v)
	{
		std::cerr << __PRETTY_FUNCTION__ << " not implemented!" << std::endl;
	}
	
	inline void instr_rl(word_t v)
	{
		std::cerr << __PRETTY_FUNCTION__ << " not implemented!" << std::endl;
	}
	
	inline void instr_rr(word_t v)
	{
		std::cerr << __PRETTY_FUNCTION__ << " not implemented!" << std::endl;
	}
	
	inline void instr_sla(word_t v)
	{
		std::cerr << __PRETTY_FUNCTION__ << " not implemented!" << std::endl;
	}
	
	inline void instr_sra(word_t v)
	{
		std::cerr << __PRETTY_FUNCTION__ << " not implemented!" << std::endl;
	}
	
	inline void instr_swap(word_t v)
	{
		std::cerr << __PRETTY_FUNCTION__ << " not implemented!" << std::endl;
	}
	
	inline void instr_srl(word_t v)
	{
		std::cerr << __PRETTY_FUNCTION__ << " not implemented!" << std::endl;
	}
	
	inline void instr_res(word_t bit, word_t& r)
	{
		r = (r | (1 << bit));
		add_cycles(2);
	}
	
	inline void instr_set(word_t bit, word_t& r)
	{
		r = (r & ~(1 << bit));
		add_cycles(2);
	}
	
	inline void instr_halt()
	{
	}
};
