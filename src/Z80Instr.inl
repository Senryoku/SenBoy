/******************************************************************************
 * Implementations of the Z80 instructions
******************************************************************************/

inline void instr_nop() { }

inline void instr_stop()
{
	_stop = true;
}

///////////////////////////////////////////////////////////////////////////////
// Arithmetic

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
	
/// Add n + Carry flag to A.
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
 * (Helper) Adds a signed 8bits integer to a 16bits integer.
**/
inline addr_t instr_add16(addr_t lhs, word_t rhs)
{
	int r = from_2c_to_signed(rhs);
	int t = lhs + r;
	set(Flag::Zero, false);
	set(Flag::Negative, false);
	if(r > 0)
	{
		set(Flag::HalfCarry, (lhs & 0xF) + (rhs & 0xF) > 0xF);
		set(Flag::Carry, (lhs & 0xFF) + rhs > 0xFF);
	} else {
		set(Flag::HalfCarry, (t & 0xF) <= (lhs & 0xF));
		set(Flag::Carry, (t & 0xFF) <= (lhs & 0xFF));
	}
	return t;
}

/**
 * Adds an immediate signed 8bits integer.
**/
inline void instr_add_sp(word_t src)
{
	_sp = instr_add16(_sp, src);
}

inline void instr_add_hl(addr_t src)
{
	set(Flag::HalfCarry, ((getHL() & 0x07FF) + (src & 0x07FF)) > 0x07FF);
	uint32_t t = getHL() + src;
	set(Flag::Carry, t > 0xFFFF);
	set_hl(t & 0xFFFF);
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
	set(Flag::Carry, _a < src);
	_a -= src;
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

/**
 * Convert A into BCD (Binary Coded Decimal)
 . (Passes Blargg's test)
**/
inline void instr_daa()
{
	int tmp = _a;
	word_t ln = _a & 0x0F;			// Low Nibble

	bool carry = false;
	if(!check(Flag::Negative))
	{
		if(check(Flag::HalfCarry) || (ln > 0x09))
		{
			tmp += 0x06;
		}
		if(check(Flag::Carry) || (tmp > 0x9F))
		{
			tmp += 0x60;
			carry = true;
		}
		//set(Flag::Carry, tmp > 0x5f);
	} else {
		if(check(Flag::HalfCarry))
		{
			tmp = (tmp - 6) & 0xFF;
		} 
		if(check(Flag::Carry))
		{
			tmp -= 0x60;
			carry = true;
		}
	}
	
	set(Flag::Carry, carry);
	set(Flag::HalfCarry, false);
	_a = tmp & 0xFF;
	set(Flag::Zero, _a == 0);
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Logical

inline void instr_and(word_t src)
{
	_a &= src;
	_f = Flag::HalfCarry | ((_a == 0) ? Flag::Zero : 0);
}

inline void instr_or(word_t src)
{
	_a |= src;
	_f = (_a == 0) ? Flag::Zero : 0;
}

inline void instr_xor(word_t src)
{
	_a ^= src;
	_f = (_a == 0) ? Flag::Zero : 0;
}

/**
 * Compare A with n. This is basically an A - n
 * subtraction instruction but the results are thrown
 * away.
 * Z - Set if result is zero. (Set if A = n.)
 * N - Set.
 * H - Set if borrow from bit 4.
 * C - Set for borrow. (Set if A < n.)
*/
inline void instr_cp(word_t src)
{
	set(Flag::HalfCarry, (_a & 0xF) < (src & 0xF));
	set(Flag::Carry, _a < src);
	set(Flag::Zero, _a == src);
	set(Flag::Negative, true);
}

///////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////
// Jumps

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
	if(b)
	{
		add_cycles(4);
		rel_jump(offset);
	}
}

inline void instr_ret()
{
	_pc = pop();
}

inline void instr_ret(bool b)
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

inline void instr_rst(word_t addr)
{
	assert(addr <= 0x38 && ((addr & 0x0F) == 0 || (addr & 0x0F) == 8));
	push(_pc);
	_pc = addr;
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Rotate

inline void instr_rlca()
{
	bool b = _a & 0x80;
	set(Flag::Carry, b);
	_a = _a << 1;
	if(b)
		_a |= 1;
	set(Flag::Zero, _a == 0);
}

inline void instr_rla()
{
	word_t tmp = check(Flag::Carry) ? 1 : 0;
	set(Flag::Carry, _a & 0x80);
	_a = _a << 1;
	_a += tmp;
	set(Flag::Zero, _a == 0);
}

inline void instr_rrca()
{
	bool b = _a & 1;
	set(Flag::Carry, b);
	_a = _a >> 1;
	if(b)
		_a |= 0x80;
	set(Flag::Zero, _a == 0);
}

inline void instr_rra()
{
	word_t tmp = check(Flag::Carry) ? 0x80 : 0;
	set(Flag::Carry, _a & 1);
	_a = _a >> 1;
	_a |= tmp;
	set(Flag::Zero, _a == 0);
}

/// Rotate n left. Old bit 0 to Carry flag.
inline void instr_rlc(word_t& v)
{
	set(Flag::Carry, v & 0x80);
	word_t t = v << 1;
	if(v & 0x80) t = t | 1;
	v = t;
	set(Flag::Zero, v == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

/// Rotate n left through Carry flag.
inline void instr_rl(word_t& v)
{
	word_t t = v << 1;
	if(check(Flag::Carry)) t = t | 1;
	set(Flag::Carry, v & 0x80);
	v = t;
	set(Flag::Zero, v == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

/// Rotate n right. Old bit 0 to Carry flag.
inline void instr_rrc(word_t& v)
{
	set(Flag::Carry, v & 1);
	word_t t = v >> 1;
	if(v & 1) t = t | 0x80;
	v = t;
	set(Flag::Zero, v == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

/// Rotate n right through Carry flag.
inline void instr_rr(word_t& v)
{
	word_t t = v >> 1;
	if(check(Flag::Carry)) t = t | 0x80;
	set(Flag::Carry, v & 1);
	v = t;
	set(Flag::Zero, v == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Shifts

/// Shift n left into Carry. LSB of n set to 0.
inline void instr_sla(word_t& v)
{
	set(Flag::Carry, _a & 0x80);
	_a = _a << 1;
	set(Flag::Zero, _a == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

/// Shift n right into Carry. MSB doesn't change.
inline void instr_sra(word_t& v)
{
	word_t t = v & 0x80;
	set(Flag::Carry, v & 1);
	v = (v >> 1) | t;
	set(Flag::Zero, v == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

/// Shift n right into Carry. MSB set to 0.
inline void instr_srl(word_t& v)
{
	set(Flag::Carry, v & 1);
	v = (v >> 1);
	set(Flag::Zero, v == 0);
	set(Flag::Negative | Flag::HalfCarry, false);
}

///////////////////////////////////////////////////////////////////////////////

/**
 * Complement A register. (Flip all bits.)
 * 
 * Z - Not affected.
 * N - Set.
 * H - Set.
 * C - Not affected.
**/
 inline void instr_cpl()
{
	_a = ~_a;
	set(Flag::Negative | Flag::HalfCarry);
}
/**
 * Complement carry flag.
 * 
 * If C flag is set, then reset it.
 * If C flag is reset, then set it.
 * Z - Not affected.
 * N - Reset.
 * H - Reset.
 * C - Complemented.
**/
inline void instr_ccf()
{
	set(Flag::Carry, !check(Flag::Carry));
	set(Flag::Negative | Flag::HalfCarry);
}

/**
 * Set Carry flag.
 * 
 * Z - Not affected.
 * N - Reset.
 * H - Reset.
 * C - Set.
**/
inline void instr_scf()
{
	set(Flag::Carry);
	set(Flag::Negative | Flag::HalfCarry);
}

inline void instr_ei()
{
	_ime = 0x01;
}

inline void instr_di()
{
	_ime = 0x00;
}

///////////////////////////////////////////////////////////////////////////////
// Bit Manipulation

/**
 * Test bit b in register r.
 *
 * Z - Set if bit b of register r is 0.
 * N - Reset.
 * H - Set.
 * C - Not affected.
**/
inline void instr_bit(word_t bit, word_t r)
{
	assert(bit < 8);
	set(Flag::Zero, r & (1 << bit));
	set(Flag::Negative, false);
	set(Flag::HalfCarry);
}

/// Set bit b in register r.
inline void instr_set(word_t bit, word_t& r)
{
	assert(bit < 8);
	r |= (1 << bit);
}

/// Reset bit b in register r.
inline void instr_res(word_t bit, word_t& r)
{
	assert(bit < 8);
	r &= ~(1 << bit);
}

///////////////////////////////////////////////////////////////////////////////

inline void instr_halt()
{
}
