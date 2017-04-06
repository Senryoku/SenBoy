#include "LR35902.hpp"

#include <cstring> // Memset

LR35902::LR35902(MMU& mmu, Gb_Apu& apu) :
	_mmu(&mmu),
	_apu(&apu)
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
	
	frame_cycles = 0;
	_ime = true;
	_stop = false;
	_halt = false;
}

void LR35902::reset_cart()
{
	_pc = 0x0100;
	_sp = 0xFFFE;
	if(_mmu->cgb_mode())
		set_af(0x11B0);
	else
		set_af(0x01B0);
	set_bc(0x0013);
	set_de(0x00D8);
	set_hl(0x014D);
	
	write(0xFF05, word_t(0x00));
	write(0xFF06, word_t(0x00));
	write(0xFF07, word_t(0x00));
	write(0xFF10, word_t(0x80));
	write(0xFF11, word_t(0xBF));
	write(0xFF12, word_t(0xF3));
	write(0xFF14, word_t(0xBF));
	write(0xFF16, word_t(0x3F));
	write(0xFF17, word_t(0x00));
	write(0xFF19, word_t(0xBF));
	write(0xFF1A, word_t(0x7F));
	write(0xFF1B, word_t(0xFF));
	write(0xFF1C, word_t(0x9F));
	write(0xFF1E, word_t(0xBF));
	write(0xFF20, word_t(0xFF));
	write(0xFF21, word_t(0x00));
	write(0xFF22, word_t(0x00));
	write(0xFF23, word_t(0xBF));
	write(0xFF24, word_t(0x77));
	write(0xFF25, word_t(0xF3));
	write(0xFF26, word_t(0xF1));
	write(0xFF40, word_t(0x91));
	write(0xFF42, word_t(0x00));
	write(0xFF43, word_t(0x00));
	write(0xFF45, word_t(0x00));
	write(0xFF47, word_t(0xFC));
	write(0xFF48, word_t(0xFF));
	write(0xFF49, word_t(0xFF));
	write(0xFF4A, word_t(0x00));
	write(0xFF4B, word_t(0x00));
	write(0xFFFF, word_t(0x00));
	
	if(!_mmu->cgb_mode())
		write(MMU::P1, word_t(0xCF)); // GB Only
	write(0xFF50, word_t(0x01)); // Disable BIOS ROM
}

bool LR35902::reached_breakpoint() const
{
	return _breakpoint;
}

void LR35902::add_breakpoint(addr_t addr)
{
	_breakpoints.push_back(addr);
}

void LR35902::rem_breakpoint(addr_t addr)
{
	_breakpoints.erase(std::find(_breakpoints.begin(), _breakpoints.end(), addr));
}

std::vector<addr_t>& LR35902::get_breakpoints()
{
	return _breakpoints;
}

void LR35902::clear_breakpoints()
{
	_breakpoints.clear();
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
		if(read(MMU::IF) & MMU::Transition)
		{
			_stop = false;
		} else {
			return;
		}
	}
	*/
	
	if(_ime) check_interrupts();
	
	/* Handles HALT instruction (Halts until a interrupt is fired).
	 *
	 * "If interrupts are disabled (DI) then
	 * halt doesn't suspend operation but it does cause
	 * the program counter to stop counting for one
	 * instruction on the GB,GBP, and SGB." 
	 */
	if(_halt)
	{
		if(_mmu->read(MMU::IF) & _mmu->read(MMU::IE))
		{
			_halt = false;
		} else { 
			add_cycles(4);
			update_timing();
			return;
		}
	}
	
	// Reads the next instruction opcode.
	word_t opcode = read(_pc++);
	add_cycles(instr_cycles[opcode]);
	
	if(_mmu->hdma_cycles())
		add_cycles(double_speed() ? 16 : 8);

	// Decode opcode (http://www.z80.info/decoding.htm)
	// And I thought I was being smart... A simple jumptable would have be mush cleaner in the end >.<"
	if(opcode == 0xCB) // Prefixed instructions
	{
		opcode = read(_pc++);
		add_cycles(instr_cycles_cb[opcode]);
		const word_t x = opcode >> 6; // bits 6 & 7
		const word_t y = (opcode >> 3) & 0b111; // bits 5 - 3
		const word_t reg = extract_src_reg(opcode);
		
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
			const addr_t addr = get_hl();
			const word_t value = read(addr);
			switch(x)
			{
				case 0b00: // Shift & Rotate
				{
					switch((opcode >> 3) & 0b111)
					{
						case 0: write(addr, instr_rlc(value)); break;
						case 1: write(addr, instr_rrc(value)); break;
						case 2: write(addr, instr_rl(value)); break;
						case 3: write(addr, instr_rr(value)); break;
						case 4: write(addr, instr_sla(value)); break;
						case 5: write(addr, instr_sra(value)); break;
						case 6: write(addr, instr_swap(value)); break;
						case 7: write(addr, instr_srl(value)); break;
					}
				}
				break;
				case 0b01: instr_bit(y, value); break;
				case 0b10: write(addr, instr_res(y, value)); break;
				case 0b11: write(addr, instr_set(y, value)); break;
				default: instr_nop(); break;
			}
						
		}
	} else {
		const word_t x = opcode >> 6; // bits 6 & 7
		const word_t y = (opcode >> 3) & 0b111; // bits 5 - 3
		
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
							write(get_hl(), instr_inc_impl(read(get_hl())));
						else
							instr_inc(_r[dst_reg]);
						break;
					}
					case 0x0D: // Same logic
					case 0x05: // DEC
					{
						word_t dst_reg = extract_dst_reg(opcode);
						if(dst_reg > 6) // LD (HL), d8
							write(get_hl(), instr_dec_impl(read(get_hl())));
						else
							instr_dec(_r[dst_reg]);
						break;
					}
					case 0x0E: // LD reg, d8
					case 0x06:
					{
						word_t dst_reg = extract_dst_reg(opcode);
						if(dst_reg > 6)	{ // LD (HL), d8
							write(get_hl(), read(_pc++));
						} else {
							_r[dst_reg] = read(_pc++);
						}
						break;
					}
					default: // Uncategorized codes
					switch(opcode)
					{
						case 0x00: instr_nop(); break;
						case 0x10: instr_stop(); break;
						case 0x20: instr_jr(!check(Flag::Zero), read(_pc++)); break;
						case 0x30: instr_jr(!check(Flag::Carry), read(_pc++)); break;
						
						case 0x01: set_bc(_mmu->read16(_pc)); _pc += 2; break;
						case 0x11: set_de(_mmu->read16(_pc)); _pc += 2; break;
						case 0x21: set_hl(_mmu->read16(_pc)); _pc += 2; break;
						case 0x31: _sp = _mmu->read16(_pc); _pc += 2; break;
						
						case 0x02: write(get_bc(), _a); break;				// LD (BC), A
						case 0x12: write(get_de(), _a); break;				// LD (DE), A
						case 0x22: write(get_hl(), _a); incr_hl(); break;	// LD (HL+), A
						case 0x32: write(get_hl(), _a); decr_hl(); break;	// LD (HL-), A
						// INC 16bits Reg
						case 0x03: set_bc(get_bc() + 1); break;
						case 0x13: set_de(get_de() + 1); break;
						case 0x23: incr_hl(); break;
						case 0x33: ++_sp; break;
						//
						case 0x07: instr_rlca(); break;
						case 0x17: instr_rla(); break;
						case 0x27: instr_daa(); break;
						case 0x37: instr_scf(); break;
						//
						case 0x08: _mmu->write16(_mmu->read16(_pc), _sp); _pc += 2; break;	// 16bits LD
						case 0x18: instr_jr(read(_pc++)); break;
						case 0x28: instr_jr(check(Flag::Zero), read(_pc++)); break;
						case 0x38: instr_jr(check(Flag::Carry), read(_pc++)); break;
						// ADD HL, 16bits Reg
						case 0x09: instr_add_hl(get_bc()); break;
						case 0x19: instr_add_hl(get_de()); break;
						case 0x29: instr_add_hl(get_hl()); break;
						case 0x39: instr_add_hl(_sp); break;
						
						case 0x0A: _a = read(get_bc()); break;
						case 0x1A: _a = read(get_de()); break;
						case 0x2A: _a = read(get_hl()); incr_hl(); break;
						case 0x3A: _a = read(get_hl()); decr_hl(); break;
						
						case 0x0B: set_bc(get_bc() - 1); break;
						case 0x1B: set_de(get_de() - 1); break;
						case 0x2B: decr_hl(); break;
						case 0x3B: --_sp; break;
						
						case 0x0F: instr_rrca(); break;
						case 0x1F: instr_rra(); break;
						case 0x2F: instr_cpl(); break;
						case 0x3F: instr_ccf(); break;
						default:
							std::cerr << "Unknown opcode: " << Hexa8(opcode) << std::endl;
							break;
					}
				}
				break;
			}
			case 0b01: // LD on registers
			{
				const word_t reg_src = extract_src_reg(opcode);
				const word_t reg_dst = extract_dst_reg(opcode);
				if(reg_src > 6 && reg_dst > 6) // (HL), (HL) => HALT !
					instr_halt();
				else if(reg_dst > 6)
					write(get_hl(), fetch_val(reg_src));
				else
					_r[reg_dst] = fetch_val(reg_src);
				break;
			}
			case 0b10: // ALU on registers
			{ 
				const word_t reg_src = extract_src_reg(opcode);
				const word_t value = fetch_val(reg_src);
				
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
						case 0xE0: write(0xFF00 + read(_pc++), _a); break;	//	LDH (n), a	
						case 0xF0: _a = read(0xFF00 + read(_pc++)); break;	//	LDH a, (n)
						// POP
						case 0xC1: set_bc(instr_pop()); break;
						case 0xD1: set_de(instr_pop()); break;
						case 0xE1: set_hl(instr_pop()); break;
						case 0xF1: set_af(instr_pop()); break;
						
						case 0xC2: _pc += 2; instr_jp(!check(Flag::Zero), _mmu->read16(_pc - 2)); break;
						case 0xD2: _pc += 2; instr_jp(!check(Flag::Carry), _mmu->read16(_pc - 2)); break;
						case 0xE2: write(0xFF00 + _c, _a); break;	// LD ($FF00+C),A
						case 0xF2: _a = read(0xFF00 + _c); break;	// LD A,($FF00+C)
						
						case 0xC3: instr_jp(_mmu->read16(_pc)); break;
						case 0xF3: instr_di(); break;
						
						case 0xC4: _pc += 2; instr_call(!check(Flag::Zero), _mmu->read16(_pc - 2)); break;
						case 0xD4: _pc += 2; instr_call(!check(Flag::Carry), _mmu->read16(_pc - 2)); break;
						
						// PUSH
						case 0xC5: instr_push(get_bc()); break;
						case 0xD5: instr_push(get_de()); break;
						case 0xE5: instr_push(get_hl()); break;
						case 0xF5: instr_push(get_af()); break;
						
						case 0xC6: instr_add(read(_pc++)); break;
						case 0xD6: instr_sub(read(_pc++)); break;
						case 0xE6: instr_and(read(_pc++)); break;
						case 0xF6: instr_or(read(_pc++)); break;
						
						case 0xC8: instr_ret(check(Flag::Zero)); break;
						case 0xD8: instr_ret(check(Flag::Carry)); break;
						case 0xE8: instr_add_sp(read(_pc++)); break;	// ADD SP, n
						case 0xF8: // LD HL,SP+r8     (16bits LD)
						{
							set_hl(add16(_sp, read(_pc++)));
							break;
						}
						
						case 0xC9: instr_ret(); break;
						case 0xD9: instr_reti(); break;
						case 0xE9: _pc = get_hl(); break;	// JP (HL)
						case 0xF9: _sp = get_hl(); break;	// LD SP, HL
						
						case 0xCA: _pc += 2; instr_jp(check(Flag::Zero), _mmu->read16(_pc - 2)); break;
						case 0xDA: _pc += 2; instr_jp(check(Flag::Carry), _mmu->read16(_pc - 2)); break;
						case 0xEA: 
							write(_mmu->read16(_pc), _a);
							_pc += 2;
							break;	// 16bits LD
						case 0xFA:
							_a = read(_mmu->read16(_pc));
							_pc += 2;
							break;
						
						case 0xFB: instr_ei(); break;
					
						case 0xCC: _pc += 2; instr_call(check(Flag::Zero), _mmu->read16(_pc - 2)); break;
						case 0xDC: _pc += 2; instr_call(check(Flag::Carry), _mmu->read16(_pc - 2)); break;
						
						case 0xCD: _pc += 2; instr_call(_mmu->read16(_pc - 2)); break;
						
						case 0xCE: instr_adc(read(_pc++)); break;
						case 0xDE: instr_sbc(read(_pc++)); break;
						case 0xEE: instr_xor(read(_pc++)); break;
						case 0xFE: instr_cp(read(_pc++)); break;
						default:
							std::cerr << "Unknown opcode: " << Hexa(opcode) << std::endl;
							break;
					}
				}
			}
		}
	}

	update_timing();
	
	_breakpoint = std::find(_breakpoints.begin(), _breakpoints.end(), _pc) != _breakpoints.end();
}
