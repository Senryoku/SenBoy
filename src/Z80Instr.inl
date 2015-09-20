/******************************************************************************
 * Implementations of the Z80 instructions
******************************************************************************/

inline void instr_nop() { add_cycles(1); }

inline void instr_stop()
{
	std::cerr << __PRETTY_FUNCTION__ << " not implemented!" << std::endl;
}

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

inline void instr_inc(word_t& src)
{
	++src;
}

inline void instr_dec(word_t& src)
{
	--src;
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
	add_cycles(1);
	set(Flag::Carry, _a & 0b10000000);
	_a = _a << 1;
	set(Flag::Zero, _a == 0);
}

inline void instr_sra(word_t v)
{
	add_cycles(1);
	set(Flag::Carry, _a & 0b00000001);
	_a = _a >> 1;
	set(Flag::Zero, _a == 0);
}

inline void instr_swap(word_t v)
{
	std::cerr << __PRETTY_FUNCTION__ << " not implemented!" << std::endl;
}

inline void instr_srl(word_t v)
{
	std::cerr << __PRETTY_FUNCTION__ << " not implemented!" << std::endl;
}

inline void instr_push(addr_t addr)
{
	push(addr);
}

inline addr_t instr_pop()
{
	add_cycles(1);
	return pop();
}

inline void instr_jp(addr_t addr)
{
	add_cycles(1);
	_pc = addr;
}

inline void instr_jp(bool b, addr_t addr)
{
	add_cycles(1);
	if(b)
		_pc = addr;
}

inline void instr_jr(word_t offset)
{
	add_cycles(1);
	rel_jump(offset);
}

inline void instr_jr(bool b, word_t offset)
{
	add_cycles(1);
	if(b) rel_jump(offset);
}
	
inline void instr_ret(bool b = true)
{
	add_cycles(1);
	if(b)
		_pc = pop();
}

inline void instr_reti()
{
	add_cycles(1);
	_pc = pop();
	write(addr_t(0xFFFF), word_t(IEFlag::All));
}

inline void instr_di()
{
	add_cycles(1);
	write(addr_t(0xFFFF), word_t(IEFlag::None));
}

inline void instr_rlca()
{
	add_cycles(1);
	set(Flag::Carry, _a & 0b10000000);
	_a = _a << 1;
	set(Flag::Zero, _a == 0);
}

inline void instr_rla()
{
	add_cycles(1);
	word_t tmp = check(Flag::Carry) ? 1 : 0;
	set(Flag::Carry, _a & 0b10000000);
	_a = _a << 1;
	_a += tmp;
	set(Flag::Zero, _a == 0);
}

inline void instr_daa() ///< @todo check
{
	add_cycles(1);
	word_t tmp = 0;
	word_t nl = _a & 0x0f;
	word_t nh = (_a & 0xf0) >> 4;

	if(!check(Flag::Negative))
	{
		if (check(Flag::HalfCarry) || (nl > 0x09))
		{
			tmp += 0x06;
		}
		if (check(Flag::Carry) || (nh > 0x09) || ((nh == 9) && (nl > 9)))
		{
			tmp += 0x60;
		}
		set(Flag::Carry, tmp > 0x5f);
	} else {
		if (!check(Flag::Carry) && check(Flag::HalfCarry))
		{
			tmp += 0xfa;
			set(Flag::Carry, false);
		} else if (check(Flag::Carry) && !check(Flag::HalfCarry)) {
			tmp += 0xa0;
			set(Flag::Carry);
		} else if (check(Flag::Carry) && check(Flag::HalfCarry)) {
			tmp += 0x9a;
			set(Flag::Carry);
		} else {
			set(Flag::Carry, false);
		}
	}
	set(Flag::Zero, tmp == 0);
	set(Flag::HalfCarry, false);
	_a -= tmp;
}

inline void instr_scf()
{
	add_cycles(1);
	set(Flag::Carry);
}

inline void instr_rrca()
{
	add_cycles(1);
	set(Flag::Carry, _a & 0b00000001);
	_a = _a >> 1;
	set(Flag::Zero, _a == 0);
}

inline void instr_rra()
{
	add_cycles(1);
	word_t tmp = check(Flag::Carry) ? 0b10000000 : 0;
	set(Flag::Carry, _a & 0b00000001);
	_a = _a >> 1;
	_a += tmp;
	set(Flag::Zero, _a == 0);
}

inline void instr_cpl()
{
	add_cycles(1);
	_a = ~_a;
	set(Flag::Negative);
	set(Flag::HalfCarry);
}

inline void instr_ccf()
{
	add_cycles(1);
	set(Flag::Carry, !check(Flag::Carry));
	set(Flag::Negative, false);
	set(Flag::HalfCarry, false);
}

inline void instr_call(addr_t addr)
{
	add_cycles(1);
	push(_pc);
	_pc = addr;
}

inline void instr_call(bool b, addr_t addr)
{
	add_cycles(1);
	if(b)
	{
		push(_pc);
		_pc = addr;
	}
}

inline void instr_rst(word_t rel_addr)
{
	add_cycles(1);
	push(_pc);
	_pc = rel_addr;
}

inline void instr_ei()
{
	add_cycles(1);
	write(addr_t(0xFFFF), word_t(IEFlag::All));
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