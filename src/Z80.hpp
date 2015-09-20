#pragma once

#include <cassert>
#include <iostream>
#include <unordered_set>

#include "ROM.hpp"
#include "GPU.hpp"

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
	
	static constexpr unsigned int	ClockRate = 4194304; // Hz
	static constexpr size_t			MemSize = 0x10000; // Bytes
	
	enum Flag : word_t
	{
		Zero = 0x80,		///< Last result was 0x00
		Negative = 0x40,	///< Last operation was a substraction
		HalfCarry = 0x20,	///< Last result was > 0x0F
		Carry = 0x10		///< Last result was > 0xFF or < 0x00
	};
	
	enum IEFlag : word_t
	{
		None = 0x00,
		VBlank = 0x01,
		LCDC = 0x02,
		TimerOverflow = 0x04,
		TransferComplete = 0x08,
		Transition = 0x10,
		All = 0xFF
	};

	ROM*	rom = nullptr;
	GPU*	gpu = nullptr;
	
	Z80();
	~Z80();
	
	/// Reset the CPU, zero-ing all registers. (Power-up state)
	void reset();
	
	/// Reset to post internal checks
	void reset_cart();
	
	/// Loads a BIOS and sets the PC to its first instruction
	bool loadBIOS(const std::string& path);
	
	void display_state()
	{
		/*
		std::cout << "PC: 0x" << std::hex << (int) (_pc - 1) << ", Excuting opcode: 0x" << std::hex << (int) read(_pc - 1) 
			<< " [0x" << (int) read(_pc) << " - 0x" << (int) read(_pc + 1) << "] AF:0x" << (int) getAF() << " BC:0x" << (int) getBC() 
				<< " DE:0x" << (int) getDE() << " HL:0x" << (int) getHL() << std::endl;
		*/
	}
	
	inline unsigned int getClockCycles() const { return _clock_cycles; }
	inline unsigned int getInstrCycles() const { return _clock_instr_cycles; }
	inline addr_t getPC() const { return _pc; }
	inline addr_t getSP() const { return _sp; }
	inline addr_t getAF() const { return (static_cast<addr_t>(_a) << 8) + _f; }
	inline addr_t getBC() const { return (static_cast<addr_t>(_b) << 8) + _c; }
	inline addr_t getDE() const { return (static_cast<addr_t>(_d) << 8) + _e; }
	inline addr_t getHL() const { return (static_cast<addr_t>(_h) << 8) + _l; }
	
	inline int getNextOpcode() const { return read(_pc); };
	inline int getNextOperand0() const { return read(_pc + 1); };
	inline int getNextOperand1() const { return read(_pc + 2); };
	
	inline bool reachedBreakpoint() const { return _breakpoint;  };
	inline void addBreakpoint(addr_t addr) { _breakpoints.insert(addr); };
	inline void clearBreakpoints() { _breakpoints.clear(); };
	
	void execute()
	{	
		_clock_instr_cycles = 0;
		_machine_instr_cycles = 0;
		
		word_t opcode = read(_pc++);
		display_state();
		// Decode opcode (http://www.z80.info/decoding.htm)
		// Check prefixes
		if(opcode == 0xCB) // Almost done @todo: Impl. S&R
		{
			opcode = read(_pc++);
			word_t x = opcode >> 6; // bits 6 & 7
			word_t y = (opcode >> 3) & 0b111; // bits 5 - 3
			word_t reg = extract_src_reg(opcode);
			
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
			
			switch(x)
			{
				case 0b00: // Done ?
				{
					switch(opcode & 0x0F)
					{
						case 0x0C: // Same logic
						case 0x04: instr_inc(fetchReg(extract_dst_reg(opcode))); break;
						case 0x0D: // Same logic
						case 0x05: instr_dec(fetchReg(extract_dst_reg(opcode))); break;
						case 0x0E: // Same logic
						case 0x06: instr_ld(fetchReg(extract_dst_reg(opcode)), read(_pc++)); break;
						default: // Uncategorized codes
						switch(opcode)
						{
							case 0x00: instr_nop(); break;
							case 0x10: instr_stop(); break;
							case 0x20: instr_jr(!check(Flag::Zero), read(_pc++)); break;
							case 0x30: instr_jr(!check(Flag::Carry), read(_pc++)); break;
							case 0x01: setBC(read16(_pc)); _pc += 2; break;
							case 0x11: setDE(read16(_pc)); _pc += 2; break;
							case 0x21: setHL(read16(_pc)); _pc += 2; break;
							case 0x31: _sp = read16(_pc); _pc += 2; break;
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
							case 0x28: instr_jr(check(Flag::Zero), read(_pc++)); break;
							case 0x38: instr_jr(check(Flag::Carry), read(_pc++)); break;
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
								std::cerr << "Unknown opcode: 0x" << std::hex << (int) opcode << std::endl;
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
					
					switch((opcode >> 3) & 0b111)
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
						case 0x04: _pc += 2; instr_call(!check(((opcode >> 4) == 0xC) ? Flag::Zero : Flag::Carry), 
												read16(_pc - 2));  break;
						case 0x07: instr_rst(y); break;
						case 0x0B: if(opcode == 0xFB) instr_ei(); break;
						case 0x0C: _pc += 2; instr_call(check(((opcode >> 4) == 0xC) ? Flag::Zero : Flag::Carry),
												read16(_pc - 2)); break;
						case 0x0D: _pc += 2; instr_call(read16(_pc - 2)); break;
						case 0x0F: instr_rst(y); break;
						default: // Uncategorized codes
						switch(opcode)
						{
							case 0xC0: instr_ret(!check(Flag::Zero)); break;
							case 0xD0: instr_ret(!check(Flag::Carry)); break;
							case 0xE0: instr_ld(rw(0xFF00 + _pc++), _a); add_cycles(2); break;			// LDH (n), a
							case 0xF0: instr_ld(_a, read(0xFF00 + read(_pc++))); add_cycles(2); break;	// LDH a, (n)
							// POP
							case 0xC1: setBC(instr_pop()); break;
							case 0xD1: setDE(instr_pop()); break;
							case 0xE1: setHL(instr_pop()); break;
							case 0xF1: setAF(instr_pop()); break;
							case 0xC2: instr_jp(!check(Flag::Zero), read16(_pc)); break;
							case 0xD2: instr_jp(!check(Flag::Carry), read16(_pc)); break;
							case 0xE2: instr_ld(rw(_c), _a); break;
							case 0xF2: instr_ld(_a, read(_c)); break;
							case 0xC3: instr_jp(read16(_pc)); break;
							case 0xF3: instr_di(); break;
							// PUSH
							case 0xC5: instr_push(getBC()); break;
							case 0xD5: instr_push(getDE()); break;
							case 0xE5: instr_push(getHL()); break;
							case 0xF5: instr_push(getAF()); break;
							case 0xC8: instr_ret(check(Flag::Zero)); break;
							case 0xD8: instr_ret(check(Flag::Carry)); break;
							case 0xE8: _sp += read(_pc++); break;
							case 0xF8: setHL(_sp + read(_pc++)); break; // 16bits LD
							case 0xC9: instr_ret(); break;
							case 0xD9: instr_reti(); break;
							case 0xE9: instr_jp(getHL()); break;
							case 0xF9: instr_ld(_sp, getHL()); break;
							case 0xCA: instr_jp(check(Flag::Zero), read16(_pc)); break;
							case 0xDA: instr_jp(check(Flag::Carry), read16(_pc)); break;
							case 0xEA: write(read16(_pc), _a); _pc += 2; break;	// 16bits LD
							case 0xFA: instr_ld(_a, read16(_pc)); _pc += 2; break;
							case 0xC6: instr_add(read(_pc++)); break;
							case 0xD6: instr_adc(read(_pc++)); break;
							case 0xE6: instr_sub(read(_pc++)); break;
							case 0xF6: instr_sbc(read(_pc++)); break;
							case 0xCE: instr_and(read(_pc++)); break;
							case 0xDE: instr_xor(read(_pc++)); break;
							case 0xEE: instr_or(read(_pc++)); break;
							case 0xFE: instr_cp(read(_pc++)); break;
							default:
								instr_nop();
								std::cerr << "Unknown opcode: 0x" << std::hex << (int) opcode << std::endl;
								break;
						}
					}
				}
			}
		}
		_clock_cycles += _clock_instr_cycles;
		
		_breakpoint = (_breakpoints.count(_pc) != 0);
	}
	
	/// Return true if the specified flag is set, false otherwise.
	inline bool check(Flag m) { return _f & m; }
	
private:
	///////////////////////////////////////////////////////////////////////////
	// Debug
	
	bool 						_breakpoint = false;
	std::unordered_set<addr_t> 	_breakpoints;
	
	///////////////////////////////////////////////////////////////////////////
	// Registers
	
	addr_t	_pc = 0;	///< Program Counter
	addr_t	_sp = 0;	///< Stack Pointer Register
	word_t	_f = 0;		///< Flags Register
	
	union ///< 8 bits general purpose registers.
	{
		struct 
		{
			word_t	_a;		///< A Register (Accumulator)
			word_t	_b;		///< B Register (Counter)
			word_t	_c;		///< C Register (Counter)
			word_t	_d;		///< D Register
			word_t	_e;		///< E Register
			word_t	_h;		///< H Register
			word_t	_l;		///< L Register
		};
		word_t _r[7];		///< Another way to access the 8 bits registers.
	};
	
	// 8/16bits registers access helpers
	inline void setHL(addr_t val) { _h = static_cast<word_t>((val >> 8) & 0xFF); _l = val & 0xFF; } 
	inline void setDE(addr_t val) { _d = (val >> 8) & 0xFF; _e = val & 0xFF; } 
	inline void setBC(addr_t val) { _b = (val >> 8) & 0xFF; _c = val & 0xFF; } 
	inline void setAF(addr_t val) { _a = (val >> 8) & 0xFF; _f = val & 0xFF; } 
	inline word_t& fetchHL() { add_cycles(1); return rw(getHL()); } // @todo replace add_cycles by read ?
	inline word_t& fetchReg(word_t n) { return (n > 6) ? fetchHL() : _r[n]; }
	inline void incrHL() { _l++; if(_l == 0) _h++; }
	inline void decrHL() { if(_l == 0) _h--; _l--; }
	
	// Flags helpers
	/// Sets (or clears if false is passed as second argument) the specified flag.
	inline void set(Flag m, bool b = true) { _f = b ? (_f | m) : (_f & ~m); }
	
	///////////////////////////////////////////////////////////////////////////
	// Cycles management
	
	unsigned int	_clock_cycles = 0;			// Clock cycles since reset
	unsigned int	_clock_instr_cycles = 0;	// Clock cycles of the last instruction
	unsigned int	_machine_instr_cycles = 0;	// Machine cycles of the last instruction
	
	inline void add_cycles(unsigned int c, unsigned int m = 0)
	{
		_clock_instr_cycles += c;
		_machine_instr_cycles += (m == 0) ? 4 * c : m;
	}
	
	///////////////////////////////////////////////////////////////////////////
	// Stack management
	
	void push(addr_t addr)
	{
		rw(_sp--) = addr & 0xFF;
		rw(_sp--) = (addr >> 8) & 0xFF;
	}
	
	addr_t pop()
	{
		assert(_sp <= 0xFFFF - 2);
		addr_t addr = 0;
		addr = static_cast<addr_t>(read(++_sp)) << 8;
		addr |= read(++_sp);
		return addr;
	}
	
	///////////////////////////////////////////////////////////////////////////
	// RAM management
	
	word_t*	_mem = nullptr;			///< Working RAM & Zero-page RAM & Cartridge RAM
	
	inline word_t read(addr_t addr) const
	{
		if(addr < 0x0100 && read(0xFF50) != 0x01) // Internal ROM
			return _mem[addr];
		else if(addr < 0x8000) // 2 * 16kB ROM Banks
			return reinterpret_cast<word_t&>(rom->read(addr));
		else if(addr < 0xA000) // VRAM
			return gpu->read(addr);
		else if(addr < 0xC000) // Cartridge RAM @todo
			return _mem[addr];
		else if(addr < 0xE000) // Working RAM (& mirror)
			return _mem[addr - 0xC000];
		else if(addr < 0xFE00) // Working RAM mirror
			return _mem[addr - 0xE000];
		else if(addr < 0xFEA0) // Sprites (OAM) @todo
			return gpu->read(addr);
		//else if(addr < 0xFF00) // I/O
		//	return _mem[addr];
		else if(addr < 0xFF4C) // I/O ports
			return _mem[addr];
		else if(addr < 0xFF80) // I/O (Video)
			return gpu->read(addr);
		else // Zero-page RAM & Interrupt Enable Register
			return _mem[addr];
	}
	
	inline word_t& rw(addr_t addr)
	{
		if(addr < 0x0100 && read(0xFF50) != 0x01) // Internal ROM
			return _mem[addr];
		else if(addr < 0x8000) // 2 * 16kB ROM Banks
			return reinterpret_cast<word_t&>(rom->read(addr));
		else if(addr < 0xA000) // VRAM
			return gpu->read(addr);
		else if(addr < 0xC000) // Cartridge RAM @todo
			return _mem[addr];
		else if(addr < 0xE000) // Working RAM (& mirror)
			return _mem[addr - 0xC000];
		else if(addr < 0xFE00) // Working RAM mirror
			return _mem[addr - 0xE000];
		else if(addr < 0xFEA0) // Sprites (OAM) @todo
			return gpu->read(addr);
		//else if(addr < 0xFF00) // I/O
		//	return _mem[addr];
		else if(addr < 0xFF40) // I/O ports
			return _mem[addr];
		else if(addr < 0xFF80) // I/O (Video)
			return gpu->read(addr);
		else // Zero-page RAM & Interrupt Enable Register
			return _mem[addr];
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
	
	///////////////////////////////////////////////////////////////////////////
	// Instructions
	
	// Helper function on opcodes
	inline word_t extract_src_reg(word_t opcode) const { return (opcode + 1) & 0b111; }
	inline word_t extract_dst_reg(word_t opcode) const { return ((opcode >> 3) + 1) & 0b111; }
	inline void rel_jump(word_t offset) { _pc += offset - ((offset & 0b10000000) ? 0x100 : 0); }
	
	#include "Z80Instr.inl"
};
