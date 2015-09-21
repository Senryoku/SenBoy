#include "Z80.hpp"

#include <cstring> // Memset

Z80::Z80()
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

void Z80::reset()
{
	_pc = 0;
	_sp = 0;
	for(int i = 0; i < 7; ++i)
		_r[i] = 0;
	_f = 0;
}

void Z80::reset_cart()
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
}

bool Z80::loadBIOS(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	
	if(!file)
	{
		std::cerr << "Error: '" << path << "' could not be opened." << std::endl;
		return false;
	}
	
	file.read((char *) mmu->getPtr(), 256);
	
	_pc = 0x0000;
	
	return true;
}

void Z80::execute()
{	
	_clock_instr_cycles = 0;
	_machine_instr_cycles = 0;
	
	check_interrupts();
	
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
				word_t& value = fetch_reg(reg);
				
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
			case 0b01: instr_bit(y, fetch_reg(reg)); break;
			case 0b10: instr_res(y, fetch_reg(reg)); break;
			case 0b11: instr_set(y, fetch_reg(reg)); break;
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
					case 0x04: 
						instr_inc(fetch_reg(extract_dst_reg(opcode))); 
						if(extract_dst_reg(opcode) > 6) add_cycles(1);
						break;
					case 0x0D: // Same logic
					case 0x05:
						instr_dec(fetch_reg(extract_dst_reg(opcode))); 
						if(extract_dst_reg(opcode) > 6) add_cycles(1);
						break;
					case 0x0E: // LD reg, d8
					case 0x06:
					{
						word_t dst_reg = extract_dst_reg(opcode);
						if(dst_reg > 6)	{ // LD (HL), d8
							mmu->write(read(getHL()), mmu->read(_pc++));
							add_cycles(3);
						} else {
							instr_ld(fetch_reg(dst_reg), mmu->read(_pc++));
							add_cycles(1);
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
						
						case 0x01: set_bc(mmu->read16(_pc)); _pc += 2; add_cycles(3); break;
						case 0x11: set_de(mmu->read16(_pc)); _pc += 2; add_cycles(3); break;
						case 0x21: set_hl(mmu->read16(_pc)); _pc += 2; add_cycles(3); break;
						case 0x31: _sp = mmu->read16(_pc); _pc += 2; add_cycles(3); break;
						
						case 0x02: mmu->write(getBC(), _a); add_cycles(1); break;	// LD (BC), A
						case 0x12: mmu->write(getDE(), _a); add_cycles(1); break;	// LD (DE), A
						case 0x22: mmu->write(getHL(), _a); incr_hl(); add_cycles(1); break;	// LD (HL+), A
						case 0x32: mmu->write(getHL(), _a); decr_hl(); add_cycles(1); break;	// LD (HL-), A
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
						case 0x18: instr_jr(read(_pc++)); break;
						case 0x28: instr_jr(check(Flag::Zero), mmu->read(_pc++)); break;
						case 0x38: instr_jr(check(Flag::Carry), mmu->read(_pc++)); break;
						// ADD HL, 16bits Reg
						case 0x09: instr_add_hl(getBC()); break;
						case 0x19: instr_add_hl(getDE()); break;
						case 0x29: instr_add_hl(getHL()); break;
						case 0x39: instr_add_hl(_sp); break;
						
						case 0x0A: instr_ld(_a, mmu->read(getBC())); add_cycles(1); break;
						case 0x1A: instr_ld(_a, mmu->read(getDE())); add_cycles(1); break;
						case 0x2A: instr_ld(_a, mmu->read(getHL())); incr_hl(); add_cycles(1); break;
						case 0x3A: instr_ld(_a, mmu->read(getHL())); decr_hl(); add_cycles(1); break;
						
						case 0x0B: set_bc(getBC() - 1); add_cycles(1); break;
						case 0x1B: set_de(getDE() - 1); add_cycles(1); break;
						case 0x2B: decr_hl(); add_cycles(1); break;
						case 0x3B: --_sp; add_cycles(1); break;
						
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
					instr_ld(fetch_reg(reg_dst), fetch_reg(reg_src));
				break;
			}
			case 0b10: // ALU on registers - Done ?
			{ 
				word_t reg_src = extract_src_reg(opcode);
				word_t value = fetch_reg(reg_src);
				
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
					case 0x04:
						_pc += 2; 
						instr_call(!check(((opcode >> 4) == 0xC) ? Flag::Zero : Flag::Carry), 
								mmu->read16(_pc - 2));  
						break;
					case 0x07: instr_rst(y); break;
					case 0x0C:
						_pc += 2; 
						instr_call(check(((opcode >> 4) == 0xC) ? Flag::Zero : Flag::Carry),
									mmu->read16(_pc - 2));
						break;
					case 0x0D:
						_pc += 2;
						instr_call(mmu->read16(_pc - 2));
						break;
					case 0x0F: instr_rst(y); break;
					default: // Uncategorized codes
					switch(opcode)
					{
						case 0xC0: instr_ret(!check(Flag::Zero)); break;
						case 0xD0: instr_ret(!check(Flag::Carry)); break;
						case 0xE0: // LDH (n), a
							mmu->write(mmu->read(0xFF00 + mmu->read(_pc++)), _a); 
							add_cycles(2); 
							break;	
						case 0xF0: // LDH a, (n)
							instr_ld(_a, read(0xFF00 + mmu->read(_pc++))); 
							add_cycles(1); 
							break;	
						// POP
						case 0xC1: set_bc(instr_pop()); add_cycles(2); break;
						case 0xD1: set_de(instr_pop()); add_cycles(2); break;
						case 0xE1: set_hl(instr_pop()); add_cycles(2); break;
						case 0xF1: set_af(instr_pop()); add_cycles(2); break;
						
						case 0xC2: instr_jp(!check(Flag::Zero), mmu->read16(_pc)); add_cycles(2); break;
						case 0xD2: instr_jp(!check(Flag::Carry), mmu->read16(_pc)); add_cycles(2); break;
						case 0xE2: mmu->write(read(_c), _a); add_cycles(2); break;
						case 0xF2: instr_ld(_a, read(_c)); break;
						
						case 0xC3: instr_jp(mmu->read16(_pc)); break;
						case 0xF3: instr_di(); break;
						// PUSH
						case 0xC5: instr_push(getBC()); break;
						case 0xD5: instr_push(getDE()); break;
						case 0xE5: instr_push(getHL()); break;
						case 0xF5: instr_push(getAF()); break;
						
						case 0xC6: instr_add(read(_pc++)); break;
						case 0xD6: instr_sub(read(_pc++)); break;
						case 0xE6: instr_and(read(_pc++)); break;
						case 0xF6: instr_or(read(_pc++)); break;
						
						case 0xC8: instr_ret(check(Flag::Zero)); break;
						case 0xD8: instr_ret(check(Flag::Carry)); break;
						case 0xE8: instr_add(_sp, read(_pc++)); break;	// ADD SP, n
						case 0xF8: set_hl(_sp + read(_pc++)); add_cycles(1); break; // 16bits LD
						
						case 0xC9: instr_ret(); break;
						case 0xD9: instr_reti(); break;
						case 0xE9: instr_jp(getHL()); break;
						case 0xF9: instr_ld(_sp, getHL()); break;
						
						case 0xCA: instr_jp(check(Flag::Zero), mmu->read16(_pc)); break;
						case 0xDA: instr_jp(check(Flag::Carry), mmu->read16(_pc)); break;
						case 0xEA: 
							mmu->write(mmu->read16(_pc), _a);
							_pc += 2;
							add_cycles(4);
							break;	// 16bits LD
						case 0xFA:
							instr_ld(_a, read(mmu->read16(_pc)));
							_pc += 2;
							add_cycles(3);
							break;
						
						case 0xFB: instr_ei(); break;
					
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
