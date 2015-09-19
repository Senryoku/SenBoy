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
	static constexpr size_t			RAMSize = 0x0400; // Bytes
	static constexpr size_t			ZPRAMSize = 0x0080; // Bytes
	
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
			word_t y = (opcode >> 3) & 0b111; // bits 5 - 3
			//word_t z = opcode & 0b111; // bits 2 - 0
			word_t p = y >> 1; // bits 5 & 4
			//word_t q = y & 1; // bit 3
			
			switch(x)
			{
				case 0b00: // Done ?
				{
					switch(opcode & 0x0F)
					{
						case 0x0C: // Same logic
						case 0x04: instr_inc(fetchReg(extract_dst_reg(opcode))); break;
						case 0x0D: // Same logic
						case 0x05: instr_dec(fetchReg(extract_dst_reg(opcode)); break;
						case 0x0E: // Same logic
						case 0x06: instr_ld(fetchReg(extract_dst_reg(opcode)), read(_pc++)); break;
						default: // Uncategorized codes
						switch(opcode)
						{
							case 0x00: instr_nop(); break;
							case 0x10: instr_stop(); break;
							case 0x20: instr_jr(Flag::Negative | Flag::Zero, read(_pc++)); break;
							case 0x30: instr_jr(Flag::Negative | Flag::Carry, read(_pc++)); break;
							case 0x01: setBC(read16(_pc)); _pc + =2; break;
							case 0x11: setDE(read16(_pc)); _pc + =2; break;
							case 0x21: setHL(read16(_pc)); _pc + =2; break;
							case 0x31: _sp = read16(_pc); _pc + =2; break;
							case 0x02: _a = read(getBC()); break;
							case 0x12: _a = read(getDE()); break;
							case 0x22: _a = read(getHL()); incrHL(); break;
							case 0x32: _a = read(getHL()); decrHL(); break;
							// INC 16bits Reg
							case 0x03: setBC(getBC() + 1); break;
							case 0x13: setDE(getDE() + 1); break;
							case 0x23: incrHL(); break;
							case 0x33: ++_sp; break;
							//
							case 0x07: instr_rlca(); break;
							case 0x17: instr_rla(); break;
							case 0x27: instr_daa(); break;
							case 0x37: instr_scf(); break;
							//
							case 0x08: write(read16(_pc), _sp); _pc += 2; break;	// 16bits LD
							case 0x18: instr_jr(read(_pc++)); break;
							case 0x28: instr_jr(Flag::Zero, read(_pc++)); break;
							case 0x38: instr_jr(Flag::Carry, read(_pc++)); break;
							// ADD HL, 16bits Reg
							case 0x09: setHL(getHL() + getBC()); add_cycles(8); break;
							case 0x19: setHL(getHL() + getDE()); add_cycles(8); break;
							case 0x29: setHL(getHL() + getHL()); add_cycles(8); break;
							case 0x39: setHL(getHL() + _sp); add_cycles(8); break;
							case 0x0A: instr_ld(_a, read(getBC())); break;
							case 0x1A: instr_ld(_a, read(getDE())); break;
							case 0x2A: instr_ld(_a, read(getHL())); incrHL(); break;
							case 0x3A: instr_ld(_a, read(getHL())); decrHL(); break;
							case 0x0B: setBC(getBC() - 1); break;
							case 0x1B: setDE(getDE() - 1); break;
							case 0x2B: decrHL(); break;
							case 0x3B: --_sp; break;
							case 0x0F: instr_rrca(); break;
							case 0x1F: instr_rra(); break;
							case 0x2F: instr_cpl(); break;
							case 0x3F: instr_ccf(); break;
							default:
								std::cerr << "Unknown opcode: " << std::hex << opcode << std::endl;
								break;
						}
					}
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
				case 0b11: // Done ?
				{
					switch(opcode & 0x0F)
					{
						case 0x04: instr_call(Flag::Negative | (((opcode >> 4) == 0xC) ? 
																	Flag::Zero : Flag::Carry), 
												read16(_pc)); _pc += 2; break;
						case 0x07: instr_rst(extract_dst_reg(opcode)); break;
						case 0x0B: if(opcode == 0xFB) instr_ei(); break;
						case 0x0C: instr_call(((opcode >> 4) == 0xC) ? Flag::Zero : Flag::Carry,
												read16(_pc)); pc += 2; break;
						case 0x0D: instr_call(read16(_pc)); pc += 2; break;
						case 0x0F: instr_rst(extract_dst_reg(opcode)); break;
						default: // Uncategorized codes
						switch(opcode)
						{
							case 0xC0: instr_ret(Flag::Negative | Flag::Zero); break;
							case 0xD0: instr_ret(Flag::Negative | Flag::Carry); break;
							case 0xE0: instr_ld(read(_pc++), _a); add_cycles(1); break;
							case 0xF0: instr_ld(_a, read(_pc++)); add_cycles(1); break;
							// POP
							case 0xC1: setBC(instr_pop()); break;
							case 0xD1: setDE(instr_pop()); break;
							case 0xE1: setHL(instr_pop()); break;
							case 0xF1: setAF(instr_pop()); break;
							case 0xC2: instr_jp(Flag::Negative | Flag::Zero, read16(_pc)); pc += 2; break;
							case 0xD2: instr_jp(Flag::Negative | Flag::Carry, read16(_pc)); pc += 2; break;
							case 0xE2: instr_ld(read(_c), _a); break;
							case 0xF2: instr_ld(_a, read(_c)); break;
							case 0xC3: instr_jp(read16(_pc)); pc += 2; break;
							case 0xF3: instr_di(); break;
							// PUSH
							case 0xC5: instr_push(getBC()); break;
							case 0xD5: instr_push(getDE()); break;
							case 0xE5: instr_push(getHL()); break;
							case 0xF5: instr_push(getAF()); break;
							case 0xC8: instr_ret(Flag::Zero); break;
							case 0xD8: instr_ret(Flag::Carry); break;
							case 0xE8: instr_add(_sp, read(_pc++)); break;
							case 0xF8: instr_ld(getHL(), _sp + read(_pc++)); break; // !!!! special case for HL T_T
							case 0xC9: instr_ret(); break;
							case 0xD9: instr_reti(); break;
							case 0xE9: instr_jp(getHL()); break;
							case 0xF9: instr_ld(_sp, getHL()); break;
							case 0xCA: instr_jp(Flag::Zero, read16(_pc)); pc += 2; break;
							case 0xDA: instr_jp(Flag::Carry, read16(_pc)); pc += 2; break;
							case 0xEA: write(read16(_pc), _a); pc += 2; break;	// 16bits LD
							case 0xFA: instr_ld(_a, read16(_pc)); pc += 2; break;
							case 0xC6: instr_add(read(_pc++)); break;
							case 0xD6: instr_adc(read(_pc++)); break;
							case 0xE6: instr_sub(read(_pc++)); break;
							case 0xF6: instr_sbc(read(_pc++)); break;
							case 0xCF: instr_and(read(_pc++)); break;
							case 0xDF: instr_xor(read(_pc++)); break;
							case 0xEF: instr_or(read(_pc++)); break;
							case 0xFF: instr_cp(read(_pc++)); break;
							default:
								instr_nop();
								std::cerr << "Unknown opcode: " << std::hex << opcode << std::endl;
								break;
						}
					}
				}
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
	
	// 8/16bits registers access helpers
	inline addr_t getHL() const { return (static_cast<addr_t>(_h) << 8) + _l; }
	inline addr_t setDE() const { return (static_cast<addr_t>(_d) << 8) + _e; }
	inline addr_t setBC() const { return (static_cast<addr_t>(_b) << 8) + _c; }
	inline addr_t setAF() const { return (static_cast<addr_t>(_a) << 8) + _f; }
	inline void setHL(addr_t val) { _h = (val >> 8) & 0xFF; _l = val & 0xFF; } 
	inline void setDE(addr_t val) { _d = (val >> 8) & 0xFF; _e = val & 0xFF; } 
	inline void setBC(addr_t val) { _b = (val >> 8) & 0xFF; _c = val & 0xFF; } 
	inline void setAF(addr_t val) { _a = (val >> 8) & 0xFF; _f = val & 0xFF; } 
	inline word_t& fetchHL() { add_cycles(1); return _mem[getHL()]; }
	inline word_t& fetchReg(word_t n) { return (n > 6) ? fetchHL() : _r[n]; }
	inline void incrHL() { _l++; if(_l == 0) _h++; }
	inline void descHL() { if(_l == 0) _h--; _l--; }
	
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
	
	word_t*	_mem;		///< Working RAM
	word_t*	_zp_mem;	///< Zero-page RAM
	
	inline word_t& read(addr_t addr)
	{
		if(addr < 0x4000) // ROM @todo
			return _mem[0];
		else if(addr < 0xA000) // VRAM @todo
			return _mem[0];
		else if(addr < 0xC000) // Cartridge RAM @todo
			return _mem[0];
		else if(addr < 0xFE00) // Working RAM (& mirror)
			return _mem[(addr - 0xC000) % RAMSize];
		else if(addr < 0xFF00) // Sprites @todo
			return _mem[0];
		else if(addr < 0xFF80) // I/O @todo
			return _mem[0];
		else // Zero-page RAM
			return _zp_mem[addr - 0xFF80];
			
		// @todo Could it work ???
		// add_cycles(1);
	}
	
	inline addr_t read16(addr_t addr)
	{
		return (static_cast<addr_t>(read(addr + 1)) << 8) + read(addr);
	}
	
	inline void	write(addr_t addr, word_t value)
	{
		if(addr < 0x4000) // ROM @todo
			(void) 0;
		else if(addr < 0xA000) // VRAM @todo
			(void) 0;
		else if(addr < 0xC000) // Cartridge RAM @todo
			(void) 0;
		else if(addr < 0xFE00) // Working RAM (& mirror)
			_mem[(addr - 0xC000) % RAMSize] = value;
		else if(addr < 0xFF00) // Sprites @todo
			(void) 0;
		else if(addr < 0xFF80) // I/O @todo
			(void) 0;
		else // Zero-page RAM
			_zp_mem[addr - 0xFF80] = value;
	}
	
	inline void	write(addr_t addr, addr_t value)
	{
		write(addr, static_cast<word_t>(value & 0xFF));
		write(addr + 1, static_cast<word_t>(value >> 8));
	}
	
	///////////////////////////////////////////////////////////////////////////
	// Instructions
	
	// Helper function on opcodes
	inline word_t extract_src_reg(word_t opcode) const { return (opcode + 1) & 0b111; }
	inline word_t extract_dst_reg(word_t opcode) const { return ((opcode >> 3) + 1) & 0b111; }
	
	inline void instr_nop() { add_cycles(1); }

	inline void instr_ld(word_t& dst, word_t src)
	{
		dst = src;
		add_cycles(1);
	}
	
	inline void instr_ld(addr_t& dst, addr_t src)
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
