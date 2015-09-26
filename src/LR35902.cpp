#include "LR35902.hpp"

#include <cstring> // Memset

size_t	LR35902::instr_length[0x100] = {
	1, 3, 1, 1, 1, 1, 2, 1, 3, 1, 1, 1, 1, 1, 2, 1,
	2, 3, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 2, 1,
	2, 3, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 2, 1,
	2, 3, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 3, 3, 3, 1, 2, 1, 1, 1, 3, 3, 3, 3, 2, 1,
	1, 1, 3, 0, 3, 1, 2, 1, 1, 1, 3, 0, 3, 0, 2, 1,
	2, 1, 2, 1, 0, 1, 2, 1, 2, 1, 3, 1, 0, 0, 2, 1
};

size_t	LR35902::instr_cycles[0x100] = {
	4, 12, 8, 8, 4, 4, 8, 4, 20, 8, 8, 8, 4, 4, 8, 4,
	4, 12, 8, 8, 4, 4, 8, 4, 12, 8, 8, 8, 4, 4, 8, 4,
	8, 12, 8, 8, 4, 4, 8, 4, 8, 8, 8, 8, 4, 4, 8, 4,
	8, 12, 8, 8, 12, 12, 12, 4, 8, 8, 8, 8, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	8, 12, 16, 12, 16, 8, 16, 8, 16, 12, 4, 12, 24, 8, 16,
	8, 12, 12, 0, 12, 16, 8, 16, 8, 16, 12, 0, 12, 0, 8, 16,
	12, 12, 8, 0, 0, 16, 8, 16, 16, 4, 16, 0, 0, 0, 8, 16,
	12, 12, 8, 4, 0, 16, 8, 16, 12, 8, 16, 4, 0, 0, 8, 16
};

size_t	LR35902::instr_cycles_cb[0x100] = {
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8,
	8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8,
	8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8,
	8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8
};

LR35902::LR35902()
{
	reset();
	
	// Alignment checks.
	assert(&_a == &_r[0]);
	assert(&_b == &_r[1]);
	assert(&_c == &_r[2]);
	assert(&_d == &_r[3]);
	assert(&_e == &_r[4]);
	assert(&_h == &_r[5]);
	assert(&_l == &_r[6]);
}

void LR35902::reset()
{
	_pc = 0;
	_sp = 0;
	for(int i = 0; i < 7; ++i)
		_r[i] = 0;
	_f = 0;
}

void LR35902::reset_cart()
{
	_pc = 0x0100;
	_sp = 0xFFFE;
	set_af(0x01B0);
	set_bc(0x0013);
	set_de(0x00D8);
	set_hl(0x014D);
	
	mmu->write(0xFF05, word_t(0x00));
	mmu->write(0xFF06, word_t(0x00));
	mmu->write(0xFF07, word_t(0x00));
	mmu->write(0xFF10, word_t(0x80));
	mmu->write(0xFF11, word_t(0xBF));
	mmu->write(0xFF12, word_t(0xF3));
	mmu->write(0xFF14, word_t(0xBF));
	mmu->write(0xFF16, word_t(0x3F));
	mmu->write(0xFF17, word_t(0x00));
	mmu->write(0xFF19, word_t(0xBF));
	mmu->write(0xFF1A, word_t(0x7F));
	mmu->write(0xFF1B, word_t(0xFF));
	mmu->write(0xFF1C, word_t(0x9F));
	mmu->write(0xFF1E, word_t(0xBF));
	mmu->write(0xFF20, word_t(0xFF));
	mmu->write(0xFF21, word_t(0x00));
	mmu->write(0xFF22, word_t(0x00));
	mmu->write(0xFF23, word_t(0xBF));
	mmu->write(0xFF24, word_t(0x77));
	mmu->write(0xFF25, word_t(0xF3));
	mmu->write(0xFF26, word_t(0xF1));
	mmu->write(0xFF40, word_t(0x91));
	mmu->write(0xFF42, word_t(0x00));
	mmu->write(0xFF43, word_t(0x00));
	mmu->write(0xFF45, word_t(0x00));
	mmu->write(0xFF47, word_t(0xFC));
	mmu->write(0xFF48, word_t(0xFF));
	mmu->write(0xFF49, word_t(0xFF));
	mmu->write(0xFF4A, word_t(0x00));
	mmu->write(0xFF4B, word_t(0x00));
	mmu->write(0xFFFF, word_t(0x00));
	
	mmu->write(MMU::P1, word_t(0xCF)); // GB Only
	mmu->write(0xFF50, word_t(0x01)); // Disable BIOS ROM
}

bool LR35902::loadBIOS(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	
	if(!file)
	{
		std::cerr << "Error: '" << path << "' could not be opened." << std::endl;
		return false;
	}
	
	file.read((char *) mmu->getPtr(), 256);
	
	_pc = 0x0000;
	mmu->write(0xFF50, word_t(0x00)); // Enable BIOS ROM
	
	return true;
}

void LR35902::execute()
{
	assert((_f & 0x0F) == 0);
	_clock_instr_cycles = 0;
	
	// Handles STOP instruction (Halts until a key is pressed)
	// @todo Really buggy at the moment - Disabled
	/*
	if(_stop)
	{
		if(mmu->read(MMU::IF) & MMU::Transition)
		{
			_stop = false;
		} else {
			return;
		}
	}
	*/
	/* Handles HALT instruction (Halts until a interrupt is fired).
	 *
	 * "If interrupts are disabled (DI) then
	 * halt doesn't suspend operation but it does cause
	 * the program counter to stop counting for one
	 * instruction on the GB,GBP, and SGB." 
	 */
	static bool repeat = false;
	static addr_t repeat_pc = 0;
	if(repeat)
	{
		repeat = false;
		_pc = repeat_pc;
	}
	
	if(_halt)
	{
		if(mmu->read(MMU::IF))
		{
			// @todo Doesn't happen in GBC mode
			if(!_ime)
			{
				repeat = true;
				repeat_pc = _pc;
			}
			
			_halt = false;
		} else { 
			update_timing();
			return;
		}
	}
	
	// Reads the next instruction opcode.
	word_t opcode = mmu->read(_pc++);
	add_cycles(instr_cycles[opcode]);
	
	display_state();
	// Decode opcode (http://www.z80.info/decoding.htm)
	// And I thought I was being smart... A simple jumptable would have be mush cleaner in the end >.<"
	if(opcode == 0xCB) // Prefixed instructions
	{
		opcode = mmu->read(_pc++);
		add_cycles(instr_cycles_cb[opcode]);
		word_t x = opcode >> 6; // bits 6 & 7
		word_t y = (opcode >> 3) & 0b111; // bits 5 - 3
		word_t reg = extract_src_reg(opcode);
		
		if(reg < 7)
		{
			word_t& r = _r[reg];
			switch(x)
			{
				case 0b00: // Shift & Rotate
				{
					
					switch((opcode >> 3) & 0b111)
					{
						case 0: r = instr_rlc(r); break;
						case 1: r = instr_rrc(r); break;
						case 2: r = instr_rl(r); break;
						case 3: r = instr_rr(r); break;
						case 4: r = instr_sla(r); break;
						case 5: r = instr_sra(r); break;
						case 6: r = instr_swap(r); break;
						case 7: r = instr_srl(r); break;
					}
				}
				break;
				case 0b01: instr_bit(y, r); break;
				case 0b10: r = instr_res(y, r); break;
				case 0b11: r = instr_set(y, r); break;
				default: instr_nop(); break;
			}
		} else { // (HL)
			addr_t addr = getHL();
			word_t value = mmu->read(getHL());
			switch(x)
			{
				case 0b00: // Shift & Rotate
				{
					switch((opcode >> 3) & 0b111)
					{
						case 0: mmu->write(addr, instr_rlc(value)); break;
						case 1: mmu->write(addr, instr_rrc(value)); break;
						case 2: mmu->write(addr, instr_rl(value)); break;
						case 3: mmu->write(addr, instr_rr(value)); break;
						case 4: mmu->write(addr, instr_sla(value)); break;
						case 5: mmu->write(addr, instr_sra(value)); break;
						case 6: mmu->write(addr, instr_swap(value)); break;
						case 7: mmu->write(addr, instr_srl(value)); break;
					}
				}
				break;
				case 0b01: instr_bit(y, value); break;
				case 0b10: mmu->write(addr, instr_res(y, value)); break;
				case 0b11: mmu->write(addr, instr_set(y, value)); break;
				default: instr_nop(); break;
			}
						
		}
	} else {
		word_t x = opcode >> 6; // bits 6 & 7
		word_t y = (opcode >> 3) & 0b111; // bits 5 - 3
		
		switch(x)
		{
			case 0b00:
			{
				switch(opcode & 0x0F)
				{
					case 0x0C: // Same logic
					case 0x04: // INC
					{
						word_t dst_reg = extract_dst_reg(opcode);
						if(dst_reg > 6) // LD (HL), d8
							mmu->write(getHL(), instr_inc_impl(mmu->read(getHL())));
						else
							instr_inc(_r[dst_reg]);
						break;
					}
					case 0x0D: // Same logic
					case 0x05: // DEC
					{
						word_t dst_reg = extract_dst_reg(opcode);
						if(dst_reg > 6) // LD (HL), d8
							mmu->write(getHL(), instr_dec_impl(mmu->read(getHL())));
						else
							instr_dec(_r[dst_reg]);
						break;
					}
					case 0x0E: // LD reg, d8
					case 0x06:
					{
						word_t dst_reg = extract_dst_reg(opcode);
						if(dst_reg > 6)	{ // LD (HL), d8
							mmu->write(getHL(), mmu->read(_pc++));
						} else {
							_r[dst_reg] = mmu->read(_pc++);
						}
						break;
					}
					default: // Uncategorized codes
					switch(opcode)
					{
						case 0x00: instr_nop(); break;
						case 0x10: instr_stop(); break;
						case 0x20: instr_jr(!check(Flag::Zero), mmu->read(_pc++)); break;
						case 0x30: instr_jr(!check(Flag::Carry), mmu->read(_pc++)); break;
						
						case 0x01: set_bc(mmu->read16(_pc)); _pc += 2; break;
						case 0x11: set_de(mmu->read16(_pc)); _pc += 2; break;
						case 0x21: set_hl(mmu->read16(_pc)); _pc += 2; break;
						case 0x31: _sp = mmu->read16(_pc); _pc += 2; break;
						
						case 0x02: mmu->write(getBC(), _a); break;				// LD (BC), A
						case 0x12: mmu->write(getDE(), _a); break;				// LD (DE), A
						case 0x22: mmu->write(getHL(), _a); incr_hl(); break;	// LD (HL+), A
						case 0x32: mmu->write(getHL(), _a); decr_hl(); break;	// LD (HL-), A
						// INC 16bits Reg
						case 0x03: set_bc(getBC() + 1); break;
						case 0x13: set_de(getDE() + 1); break;
						case 0x23: incr_hl(); break;
						case 0x33: ++_sp; break;
						//
						case 0x07: instr_rlca(); break;
						case 0x17: instr_rla(); break;
						case 0x27: instr_daa(); break;
						case 0x37: instr_scf(); break;
						//
						case 0x08: mmu->write(mmu->read16(_pc), _sp); _pc += 2; break;	// 16bits LD
						case 0x18: instr_jr(mmu->read(_pc++)); break;
						case 0x28: instr_jr(check(Flag::Zero), mmu->read(_pc++)); break;
						case 0x38: instr_jr(check(Flag::Carry), mmu->read(_pc++)); break;
						// ADD HL, 16bits Reg
						case 0x09: instr_add_hl(getBC()); break;
						case 0x19: instr_add_hl(getDE()); break;
						case 0x29: instr_add_hl(getHL()); break;
						case 0x39: instr_add_hl(_sp); break;
						
						case 0x0A: _a = mmu->read(getBC()); break;
						case 0x1A: _a = mmu->read(getDE()); break;
						case 0x2A: _a = mmu->read(getHL()); incr_hl(); break;
						case 0x3A: _a = mmu->read(getHL()); decr_hl(); break;
						
						case 0x0B: set_bc(getBC() - 1); break;
						case 0x1B: set_de(getDE() - 1); break;
						case 0x2B: decr_hl(); break;
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
			case 0b01: // LD on registers
			{
				word_t reg_src = extract_src_reg(opcode);
				word_t reg_dst = extract_dst_reg(opcode);
				if(reg_src > 6 && reg_dst > 6) // (HL), (HL) => HALT !
					instr_halt();
				else if(reg_dst > 6)
					mmu->write(getHL(), fetch_val(reg_src));
				else
					_r[reg_dst] = fetch_val(reg_src);
				break;
			}
			case 0b10: // ALU on registers
			{ 
				word_t reg_src = extract_src_reg(opcode);
				word_t value = fetch_val(reg_src);
				
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
			case 0b11:
			{
				switch(opcode & 0x0F)
				{
					case 0x07: instr_rst(y * 0x08); break;
					case 0x0F: instr_rst(y * 0x08); break;
					default: // Uncategorized codes
					switch(opcode)
					{
						case 0xC0: instr_ret(!check(Flag::Zero)); break;
						case 0xD0: instr_ret(!check(Flag::Carry)); break;
						case 0xE0: mmu->write(0xFF00 + mmu->read(_pc++), _a); break;	//	LDH (n), a	
						case 0xF0: _a = mmu->read(0xFF00 + mmu->read(_pc++)); break;	//	LDH a, (n)
						// POP
						case 0xC1: set_bc(instr_pop()); break;
						case 0xD1: set_de(instr_pop()); break;
						case 0xE1: set_hl(instr_pop()); break;
						case 0xF1: set_af(instr_pop()); break;
						
						case 0xC2: _pc += 2; instr_jp(!check(Flag::Zero), mmu->read16(_pc - 2)); break;
						case 0xD2: _pc += 2; instr_jp(!check(Flag::Carry), mmu->read16(_pc - 2)); break;
						case 0xE2: mmu->write(0xFF00 + _c, _a); break;	// LD ($FF00+C),A
						case 0xF2: _a = mmu->read(0xFF00 + _c); break;	// LD A,($FF00+C)
						
						case 0xC3: instr_jp(mmu->read16(_pc)); break;
						case 0xF3: instr_di(); break;
						
						case 0xC4: _pc += 2; instr_call(!check(Flag::Zero), mmu->read16(_pc - 2)); break;
						case 0xD4: _pc += 2; instr_call(!check(Flag::Carry), mmu->read16(_pc - 2)); break;
						
						// PUSH
						case 0xC5: instr_push(getBC()); break;
						case 0xD5: instr_push(getDE()); break;
						case 0xE5: instr_push(getHL()); break;
						case 0xF5: instr_push(getAF()); break;
						
						case 0xC6: instr_add(mmu->read(_pc++)); break;
						case 0xD6: instr_sub(mmu->read(_pc++)); break;
						case 0xE6: instr_and(mmu->read(_pc++)); break;
						case 0xF6: instr_or(mmu->read(_pc++)); break;
						
						case 0xC8: instr_ret(check(Flag::Zero)); break;
						case 0xD8: instr_ret(check(Flag::Carry)); break;
						case 0xE8: instr_add_sp(mmu->read(_pc++)); break;	// ADD SP, n
						case 0xF8:	//LD HL,SP+r8     (16bits LD)
						{
							set_hl(add16(_sp, mmu->read(_pc++)));
							break;
						}
						
						case 0xC9: instr_ret(); break;
						case 0xD9: instr_reti(); break;
						case 0xE9: _pc = getHL(); break;	// JP (HL)
						case 0xF9: _sp = getHL(); break;	// LD SP, HL
						
						case 0xCA: _pc += 2; instr_jp(check(Flag::Zero), mmu->read16(_pc - 2)); break;
						case 0xDA: _pc += 2; instr_jp(check(Flag::Carry), mmu->read16(_pc - 2)); break;
						case 0xEA: 
							mmu->write(mmu->read16(_pc), _a);
							_pc += 2;
							break;	// 16bits LD
						case 0xFA:
							_a = mmu->read(mmu->read16(_pc));
							_pc += 2;
							break;
						
						case 0xFB: instr_ei(); break;
					
						case 0xCC: _pc += 2; instr_call(check(Flag::Zero), mmu->read16(_pc - 2)); break;
						case 0xDC: _pc += 2; instr_call(check(Flag::Carry), mmu->read16(_pc - 2)); break;
						
						case 0xCD: _pc += 2; instr_call(mmu->read16(_pc - 2)); break;
						
						case 0xCE: instr_adc(mmu->read(_pc++)); break;
						case 0xDE: instr_sbc(mmu->read(_pc++)); break;
						case 0xEE: instr_xor(mmu->read(_pc++)); break;
						case 0xFE: instr_cp(mmu->read(_pc++)); break;
						default:
							instr_nop();
							std::cerr << "Unknown opcode: 0x" << std::hex << (int) opcode << std::endl;
							break;
					}
				}
			}
		}
	}
	//assert(_clock_instr_cycles > 0);
	_clock_cycles += _clock_instr_cycles;
	
	update_timing();
	
	check_interrupts();
	
	_breakpoint = (_breakpoints.count(_pc) != 0);
}
