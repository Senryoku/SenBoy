/******************************************************************************
 * Implementations of the Z80 instructions
******************************************************************************/

inline void instr_nop() { }

inline void instr_stop()
{
	_stop = true;
}

/**
 * Z - Set if result is zero.
 * N - Reset.
 * H - Set if carry from bit 3.
 * C - Set if carry from bit 7.
**/
inline void instr_add(word_t src)
{
	set(Flag::HalfCarry, ((_a & 0xF) + (src & 0xF)) > 0x0F);
	uint16_t t = _a + src;
	set(Flag::Carry, t > 0xFF);
	_a = t & 0xFF;
	set(Flag::Zero, _a == 0);
	set(Flag::Negative, false);
}

inline void instr_add_sp(word_t src)
{
	set(Flag::HalfCarry, ((_sp & 0x07FF) + src) > 0x07FF);
	uint32_t t = _sp + src;
	set(Flag::Carry, t > 0xFFFF);
	_sp = t & 0xFFFF;
	set(Flag::Zero | Flag::Negative, false);
}

inline void instr_add_hl(addr_t src)
{
	set(Flag::HalfCarry, ((getHL() & 0x07FF) + (src & 0x07FF)) > 0x07FF);
	uint32_t t = getHL() + src;
	set(Flag::Carry, t > 0xFFFF);
	set_hl(t & 0xFFFF);
	set(Flag::Negative, false);
}
		
inline void instr_adc(word_t src)
{
	set(Flag::HalfCarry, ((((_a & 0xF) + (src & 0xF) + (check(Flag::Carry) ? 1 : 0)) > 0xF)));
	uint16_t t = _a + src + (check(Flag::Carry) ? 1 : 0);
	set(Flag::Carry, t > 0xFF);
	_a = t & 0xFF;
	set(Flag::Zero, _a == 0);
	set(Flag::Negative, false);
}

/**
 * Subtract n from A.
 * Z - Set if result is zero.
 * N - Set.
 * H - Set if borrow from bit 4.
 * C - Set if borrow.
**/
inline void instr_sub(word_t src)
{
	set(Flag::HalfCarry, (_a & 0xF) < (src & 0xF));
	int16_t t = _a - src;
	set(Flag::Carry, t < 0x00);
	_a = t & 0xFF;
	set(Flag::Zero, _a == 0);
	set(Flag::Negative, true);
}

/**
 * Subtract n + Carry flag from A.
 * Z - Set if result is zero.
 * N - Set.
 * H - Set if borrow from bit 4.
 * C - Set if borrow.
**/
inline void instr_sbc(word_t src)
{
	set(Flag::HalfCarry, (_a & 0xF) < (src & 0xF) + (check(Flag::Carry) ? 1 : 0));
	int16_t t = _a - src - (check(Flag::Carry) ? 1 : 0);
	set(Flag::Carry, t < 0x00);
	_a = t & 0xFF;
	set(Flag::Zero, _a == 0);
	set(Flag::Negative, true);
}

inline void instr_inc(word_t& src)
{
	set(Flag::HalfCarry, (src & 0x0F) == 0x0F);
	++src;
	set(Flag::Zero, src == 0);
	set(Flag::Negative, false);
}

inline void instr_dec(word_t& src)
{
	set(Flag::HalfCarry, (src & 0x0F) == 0x00);
	--src;
	set(Flag::Zero, src == 0);
	set(Flag::Negative, true);
}

inline void instr_and(word_t src)
{
	_a &= src;
	_f = Flag::HalfCarry | ((_a == 0) ? Flag::Zero : 0);
}

inline void instr_xor(word_t src)
{
	_a ^= src;
	_f = (_a == 0) ? Flag::Zero : 0;
}

inline void instr_or(word_t src)
{
	_a |= src;
	_f = (_a == 0) ? Flag::Zero : 0;
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
	set(Flag::HalfCarry, (_a & 0xF) < (src & 0xF));
	int16_t t = _a - src;
	set(Flag::Carry, t < 0x00);
	set(Flag::Zero, t == 0);
	set(Flag::Negative, true);
}

inline void instr_bit(word_t bit, word_t r)
{
	set(Flag::Zero, r & (1 << bit));
	set(Flag::Negative, false);
	set(Flag::HalfCarry);
}

/// Rotate n left. Old bit 0 to Carry flag.
inline void instr_rlc(word_t& v)
{
	set(Flag::Carry, v & 0b10000000);
	word_t t = v << 1;
	if(v & 0b10000000) t = t | 0b00000001;
	v = t;
	set(Flag::Zero, v == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

/// Rotate n left through Carry flag.
inline void instr_rl(word_t& v)
{
	word_t t = v << 1;
	if(check(Flag::Carry)) t = t | 0b00000001;
	set(Flag::Carry, v & 0b10000000);
	v = t;
	set(Flag::Zero, v == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

/// Rotate n right. Old bit 0 to Carry flag.
inline void instr_rrc(word_t& v)
{
	set(Flag::Carry, v & 0b00000001);
	word_t t = v >> 1;
	if(v & 0b00000001) t = t | 0b10000000;
	v = t;
	set(Flag::Zero, v == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

/// Rotate n right through Carry flag.
inline void instr_rr(word_t& v)
{
	word_t t = v >> 1;
	if(check(Flag::Carry)) t = t | 0b10000000;
	set(Flag::Carry, v & 0b00000001);
	v = t;
	set(Flag::Zero, v == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

/// Shift n left into Carry. LSB of n set to 0.
inline void instr_sla(word_t& v)
{
	set(Flag::Carry, _a & 0b10000000);
	_a = _a << 1;
	set(Flag::Zero, _a == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

/// Shift n right into Carry. MSB set to 0.
inline void instr_srl(word_t& v)
{
	set(Flag::Carry, v & 0b00000001);
	v = (v >> 1);
	set(Flag::Zero, v == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

inline void instr_sra(word_t& v)
{
	word_t t = v & 0b10000000;
	set(Flag::Carry, v & 0b00000001);
	v = (v >> 1) | t;
	set(Flag::Zero, v == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

/**
 * Swaps nibbles (groups of 4bits) of n.
**/
inline void instr_swap(word_t& v)
{
	word_t t = v & 0x0F;
	v = ((v << 4) & 0xF0) | t;
	_f = (v == 0) ? Flag::Zero : 0;
}

inline void instr_push(addr_t addr)
{
	push(addr);
}

inline addr_t instr_pop()
{
	return pop();
}

inline void instr_jp(addr_t addr)
{
	_pc = addr;
}

inline void instr_jp(bool b, addr_t addr)
{
	if(b)
	{
		add_cycles(4);
		_pc = addr;
	}
}

inline void instr_jr(word_t offset)
{
	rel_jump(offset);
}

inline void instr_jr(bool b, word_t offset)
{
	if(b) rel_jump(offset);
}
	
inline void instr_ret(bool b = true)
{
	if(b)
	{
		add_cycles(12);
		_pc = pop();
	}
}

inline void instr_reti()
{
	_pc = pop();
	_ime = 0x01;
}

inline void instr_rlca()
{
	set(Flag::Carry, _a & 0b10000000);
	_a = _a << 1;
	set(Flag::Zero, _a == 0);
}

inline void instr_rla()
{
	word_t tmp = check(Flag::Carry) ? 1 : 0;
	set(Flag::Carry, _a & 0b10000000);
	_a = _a << 1;
	_a += tmp;
	set(Flag::Zero, _a == 0);
}

inline void instr_daa() ///< @todo check
{
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
	set(Flag::Carry);
}

inline void instr_rrca()
{
	set(Flag::Carry, _a & 0b00000001);
	_a = _a >> 1;
	set(Flag::Zero, _a == 0);
}

inline void instr_rra()
{
	word_t tmp = check(Flag::Carry) ? 0b10000000 : 0;
	set(Flag::Carry, _a & 0b00000001);
	_a = _a >> 1;
	_a += tmp;
	set(Flag::Zero, _a == 0);
}

inline void instr_cpl()
{
	_a = ~_a;
	set(Flag::Negative | Flag::HalfCarry);
}

inline void instr_ccf()
{
	set(Flag::Carry, !check(Flag::Carry));
	set(Flag::Negative, false);
	set(Flag::HalfCarry, false);
}

inline void instr_call(addr_t addr)
{
	push(_pc);
	_pc = addr;
}

inline void instr_call(bool b, addr_t addr)
{
	if(b)
	{
		push(_pc);
		_pc = addr;
		add_cycles(12);
	}
}

inline void instr_rst(word_t rel_addr)
{
	push(_pc);
	_pc = rel_addr;
}

inline void instr_ei()
{
	_ime = 0x01;
}

inline void instr_di()
{
	_ime = 0x00;
}

inline void instr_res(word_t bit, word_t& r)
{
	r = (r & ~(1 << bit));
}

inline void instr_set(word_t bit, word_t& r)
{
	r = (r | (1 << bit));
}

inline void instr_halt()
{
}
