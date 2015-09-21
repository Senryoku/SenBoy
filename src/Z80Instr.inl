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

/**
 * Z - Set if result is zero.
 * N - Reset.
 * H - Set if carry from bit 3.
 * C - Set if carry from bit 7.
**/
inline void instr_add(word_t src)
{
	set(Flag::HalfCarry, ((_a & 0xF) + (src & 0xF)) & 0x10);
	uint16_t t = _a + src;
	set(Flag::Carry, t > 0xFF);
	_a = t & 0xFF;
	set(Flag::Zero, _a == 0);
	set(Flag::Negative, false);
	add_cycles(1);
}

inline void instr_add(addr_t& reg16, word_t src)
{
	set(Flag::HalfCarry, ((reg16 & 0xFF) + (src & 0xFF)) & 0x0100);
	uint32_t t = reg16 + src;
	set(Flag::Carry, t > 0xFFFF);
	reg16 = t;
	set(Flag::Zero, false);
	set(Flag::Negative, false);
	add_cycles(3);
}

inline void instr_add_hl(addr_t src)
{
	set(Flag::Negative,  false);
	set(Flag::HalfCarry, ((getHL() & 0xFF) + (src & 0xFF)) & 0x100);
	set(Flag::Carry, (static_cast<uint32_t>(getHL()) + src) > 0xFFFF);
	set_hl(getHL() + src);
	add_cycles(1);
}
		
inline void instr_adc(word_t src)
{
	set(Flag::HalfCarry, ((((_a & 0xF) + (src & 0xF) + (check(Flag::Carry) ? 1 : 0)) & 0x10)));
	uint16_t t = _a + src + (check(Flag::Carry) ? 1 : 0);
	set(Flag::Carry, t > 0xFF);
	_a = t & 0xFF;
	set(Flag::Zero, _a == 0);
	set(Flag::Negative, false);
	add_cycles(1);
}

/**
 * Subtract n from A.
 * Z - Set if result is zero.
 * N - Set.
 * H - Set if no borrow from bit 4.
 * C - Set if no borrow.
**/
inline void instr_sub(word_t src)
{
	set(Flag::HalfCarry, ((_a & 0xF) - (src & 0xF)) == 0x0F);
	int16_t t = _a - src;
	set(Flag::Carry, t < 0x00);
	_a = t & 0xFF;
	set(Flag::Zero, _a == 0);
	set(Flag::Negative, true);
	add_cycles(1);
}

/**
 * Subtract n + Carry flag from A.
 * Z - Set if result is zero.
 * N - Set.
 * H - Set if no borrow from bit 4.
 * C - Set if no borrow.
**/
inline void instr_sbc(word_t src)
{
	set(Flag::HalfCarry, (((_a & 0xF) - (src & 0xF) - (check(Flag::Carry) ? 1 : 0)) == 0x0F));
	int16_t t = _a - src - (check(Flag::Carry) ? 1 : 0);
	set(Flag::Carry, t < 0x00);
	_a = t & 0xFF;
	set(Flag::Zero, _a == 0);
	set(Flag::Negative, true);
	add_cycles(1);
}

inline void instr_inc(word_t& src)
{
	++src;
	set(Flag::Zero, src == 0);
	set(Flag::Negative, false);
	set(Flag::HalfCarry, (src & 0x0F) == 0x00);
}

inline void instr_dec(word_t& src)
{
	--src;
	set(Flag::Zero, src == 0);
	set(Flag::Negative, true);
	set(Flag::HalfCarry, (src & 0x0F) == 0x0F);
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

/**
 * Compare A with n. This is basically an A - n
 * subtraction instruction but the results are thrown
 * away.
 * Z - Set if result is zero. (Set if A = n.)
 * N - Set.
 * H - Set if no borrow from bit 4.
 * C - Set for no borrow. (Set if A < n.)
*/
inline void instr_cp(word_t src)
{
	set(Flag::HalfCarry, ((_a & 0xF) - (src & 0xF)) == 0x0F);
	int16_t t = _a - src;
	set(Flag::Carry, t < 0x00);
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

/**
 * Swaps nibbles (groups of 4bits) of n.
**/
inline void instr_swap(word_t& v)
{
	word_t t = v & 0x0F;
	v = ((v << 4) & 0xF0) | t;
	set(Flag::Zero, v == 0);
	set(Flag::Negative, false);
	set(Flag::HalfCarry, false);
	set(Flag::Carry, false);
}

/**
 * Shift n right into Carry. MSB set to 0.
**/
inline void instr_srl(word_t& v)
{
	set(Flag::Zero, v == 0);
	v = v >> 1;
	set(Flag::Negative, false);
	set(Flag::HalfCarry, false);
	set(Flag::Carry, v & 0b0000001);
}

inline void instr_push(addr_t addr)
{
	add_cycles(4);
	push(addr);
}

inline addr_t instr_pop()
{
	add_cycles(3);
	return pop();
}

inline void instr_jp(addr_t addr)
{
	add_cycles(4);
	_pc = addr;
}

inline void instr_jp(bool b, addr_t addr)
{
	add_cycles(b ? 4 : 3);
	if(b)
		_pc = addr;
}

inline void instr_jr(word_t offset)
{
	add_cycles(2);
	rel_jump(offset);
}

inline void instr_jr(bool b, word_t offset)
{
	add_cycles(2);
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
	_ime = 0x01;
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
	_ime = 0x01;
}

inline void instr_di()
{
	add_cycles(1);
	_ime = 0x00;
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
