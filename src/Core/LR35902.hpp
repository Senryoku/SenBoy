#pragma once

#include <vector>

#include <gb_apu/Gb_Apu.h>

#include <Core/MMU.hpp>

/**
 * Gameboy CPU (Sharp LR35902)
**/
class LR35902
{
public:
	static constexpr unsigned int	ClockRate = 4194304; // Hz
	
	enum Flag : word_t
	{
		Zero      = 0x80,		///< Last result was 0x00
		Negative  = 0x40,	    ///< Last operation was a substraction
		HalfCarry = 0x20,	    ///< Last result was > 0x0F
		Carry     = 0x10		///< Last result was > 0xFF or < 0x00
	};

	size_t  frame_cycles = 0;
	
	LR35902(MMU& _mmu, Gb_Apu& _apu);
	~LR35902() =default;
	
	explicit LR35902(const LR35902& rhs) {
		*this = rhs;
	}
	LR35902& operator=(const LR35902& rhs) {
		_pc = rhs._pc;
		_sp = rhs._sp;
		_f = rhs._f;
		
		for(int i = 0; i < 7; ++i)
			_r[i] = rhs._r[i];
		
		_ime = rhs._ime;
		_stop = rhs._stop;
		_halt = rhs._halt;
		
		return *this;
	}
	
	/// Reset the CPU, zero-ing all registers. (Power-up state)
	void reset();
	
	/// Reset to post internal checks
	void reset_cart();
	
	inline bool double_speed() const { return (_mmu->read(MMU::KEY1) & 0x80); }
	inline uint64_t get_clock_cycles() const { return _clock_cycles; }
	inline uint64_t get_instr_cycles() const { return _clock_instr_cycles; }
	inline addr_t get_pc() const { return _pc; }
	inline addr_t get_sp() const { return _sp; }
	inline addr_t get_af() const { return (static_cast<addr_t>(_a) << 8) + _f; }
	inline addr_t get_bc() const { return (static_cast<addr_t>(_b) << 8) + _c; }
	inline addr_t get_de() const { return (static_cast<addr_t>(_d) << 8) + _e; }
	inline addr_t get_hl() const { return (static_cast<addr_t>(_h) << 8) + _l; }
	
	inline std::string get_disassembly() const;
	inline std::string get_disassembly(addr_t addr) const;
	inline int get_next_opcode() const { return read(_pc); };
	inline int get_next_operand0() const { return read(_pc + 1); };
	inline int get_next_operand1() const { return read(_pc + 2); };
	
	bool reached_breakpoint() const;
	void add_breakpoint(addr_t addr);
	void rem_breakpoint(addr_t addr);
	std::vector<addr_t>& get_breakpoints();
	void clear_breakpoints();
	
	void execute();
	
	/// Return true if the specified flag is set, false otherwise.
	inline bool check(Flag m) { return _f & m; }
	
	// Static
	/// Length for each instruction (in bytes)
	static size_t	instr_length[0x100];
	/// Cycle count for each instruction (Some jump may cost more)
	static size_t	instr_cycles[0x100];
	/// Cycle count for each 0xCB prefixed instruction
	static size_t	instr_cycles_cb[0x100];
	
	static std::string	instr_str[0x100];
	static std::string	instr_cb_str[0x100];
	
private:
	MMU* const		_mmu = nullptr;
	Gb_Apu* const	_apu = nullptr;
	
	///////////////////////////////////////////////////////////////////////////
	// Debug
	
	bool 					_breakpoint = false;
	std::vector<addr_t> 	_breakpoints;
	
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
	
	bool	_ime = true; 	// Interrupt Master Enable
	
	bool	_stop = false;	// instr_stop
	bool	_halt = false;	// instr_halt
	
	// 8/16bits registers access helpers
	inline void set_hl(addr_t val) { _h = (val >> 8) & 0xFF; _l = val & 0xFF; } 
	inline void set_de(addr_t val) { _d = (val >> 8) & 0xFF; _e = val & 0xFF; } 
	inline void set_bc(addr_t val) { _b = (val >> 8) & 0xFF; _c = val & 0xFF; } 
	inline void set_af(addr_t val) { _a = (val >> 8) & 0xFF; _f = val & 0xF0; } // Low nibble of F is always 0!
		
	inline word_t fetch_hl_val() { return read(get_hl()); }
	inline word_t fetch_val(word_t n) { return (n > 6) ? fetch_hl_val() : _r[n]; }
	
	inline void incr_hl() { _l++; _h += _l == 0; }
	inline void decr_hl() { _h -= _l == 0; _l--; }
	
	// Flags helpers
	/// Sets (or clears if false is passed as second argument) the specified flag.
	inline void set(Flag m, bool b = true)   { set(static_cast<word_t>(m), b); }
	inline void set(word_t m, bool b = true) { _f = b ? (_f | m) : (_f & ~m); }
	
	// APU Intercept
	inline word_t read(addr_t addr) const;
	inline void write(addr_t addr, word_t value);
	
	///////////////////////////////////////////////////////////////////////////
	// Interrupts management
	
	inline void exec_interrupt(MMU::InterruptFlag i, addr_t addr);
	inline void update_timing();
	inline void check_interrupts();
	
	///////////////////////////////////////////////////////////////////////////
	// Cycles management
	
	unsigned int _clock_cycles = 0;			///< Clock cycles since reset
	unsigned int _clock_instr_cycles = 0;	///< Clock cycles of the last instruction
	unsigned int _divider_register = 0;		///< Cycles not yet counted in DIV
	unsigned int _timer_counter = 0;		///< Cycles not yet counted in TIMA
	
	inline void add_cycles(unsigned int c);
	
	///////////////////////////////////////////////////////////////////////////
	// Stack management
	
	inline void push(addr_t addr);
	inline addr_t pop();
	
	#include "LR35902Instr.inl"
};

///////////////////////////////////////////////////////////////////////////////
// Implementations of inlined functions

inline std::string LR35902::get_disassembly() const
{
	return get_disassembly(_pc);
}

inline std::string LR35902::get_disassembly(addr_t addr) const
{
	word_t op = read(addr);
	std::string r = (op == 0xCB) ? instr_cb_str[read(addr + 1)] : instr_str[op];
	size_t length = (op == 0xCB) ? 2 : instr_length[op];
	addr_t operand = (op == 0xCB) ? addr + 2 : addr + 1;
	size_t p = std::string::npos;
	if(length > 2)
	{
		if((p = r.find(" a16")) != std::string::npos) r.insert(p + 4, " = " + Hexa(_mmu->read16(operand)).str());
		if((p = r.find(",a16")) != std::string::npos) r.insert(p + 4, " = " + Hexa(_mmu->read16(operand)).str());
		if((p = r.find("(a16)")) != std::string::npos) r.insert(p + 5, " = " + Hexa8(read(_mmu->read16(operand))).str());
		if((p = r.find("d16")) != std::string::npos) r.insert(p + 3, " = " + Hexa(_mmu->read16(operand)).str());
	} else if(length > 1 && op != 0xCB) {
		if((p = r.find("a8")) != std::string::npos) r.insert(p + 2, " = " + Hexa(0xFF00 + _mmu->read(operand)).str()
							+ " => " + Hexa8(_mmu->read(0xFF00 + _mmu->read(operand))).str());
		if((p = r.find("d8")) != std::string::npos) r.insert(p + 2, " = " + Hexa8(_mmu->read(operand)).str());
		if((p = r.find("r8")) != std::string::npos) r.insert(p + 2, " = " + Hexa8(from_2c_to_signed(_mmu->read(operand))).str());
	}
	if((p = r.find("(BC)")) != std::string::npos) r.insert(p + 4, " = " + Hexa8(_mmu->read(get_bc())).str());
	if((p = r.find("(DE)")) != std::string::npos) r.insert(p + 4, " = " + Hexa8(_mmu->read(get_de())).str());
	if((p = r.find("(HL)")) != std::string::npos) r.insert(p + 4, " = " + Hexa8(_mmu->read(get_hl())).str());
	if((p = r.find("(HL+)")) != std::string::npos) r.insert(p + 5, " = " + Hexa8(_mmu->read(get_hl())).str());
	if((p = r.find("(HL-)")) != std::string::npos) r.insert(p + 5, " = " + Hexa8(_mmu->read(get_hl())).str());
	return r;
};
	
inline word_t LR35902::read(addr_t addr) const
{
	return in_range(addr, Gb_Apu::start_addr, Gb_Apu::end_addr) ?
		_apu->read_register(frame_cycles, addr) :
		_mmu->read(addr);
}

inline void LR35902::write(addr_t addr, word_t value)
{
	if(in_range(addr, Gb_Apu::start_addr, Gb_Apu::end_addr))
		_apu->write_register(frame_cycles, addr, value);
	else
		_mmu->write(addr, value);
}

inline void LR35902::exec_interrupt(MMU::InterruptFlag i, addr_t addr)
{
	push(_pc);
	_pc = addr;
	_ime = false;
	add_cycles(20);
	_mmu->rw_reg(MMU::IF) &= ~i;
	_halt = false;
}

inline void LR35902::update_timing()
{
	_clock_cycles += _clock_instr_cycles;
	
	// Updates at 16384Hz, which is ClockRate / 256. (Affected by double speed mode)
	_divider_register += _clock_instr_cycles;
	if(_divider_register >= 256)
	{
		_divider_register -= 256;
		++_mmu->rw_reg(MMU::DIV);
	}
	
	// TAC: Timer Control
	// Bit 2   : Timer Enable
	// Bit 1-0 : Select clock diviser (16 to 1024)
	const word_t TAC = _mmu->read(MMU::TAC);
	if(TAC & 0b100) // Is TIMA timer enabled?
	{
		_timer_counter += _clock_instr_cycles;
		constexpr unsigned int Divisors[4]{1024, 16, 64, 256};
		const unsigned int tac_divisor = Divisors[TAC & 0b11];
		while(_timer_counter >= tac_divisor)
		{
			_timer_counter -= tac_divisor;
			if(_mmu->read(MMU::TIMA) != 0xFF)
			{
				++_mmu->rw_reg(MMU::TIMA);
			} else {
				_mmu->rw_reg(MMU::TIMA) = _mmu->read(MMU::TMA);
				// Request the timer interrupt
				_mmu->rw_reg(MMU::IF) |= MMU::TimerOverflow;
			}
		}
	}
}

inline void LR35902::check_interrupts()
{
	const word_t IF = _mmu->read(MMU::IF);
	const word_t IE = _mmu->read(MMU::IE);
	const word_t WaitingInt = IE & IF;
	if(WaitingInt) // An enabled interrupt is waiting
	{
		// Check each interrupt in order of priority.
		// If requested and enabled, push current PC, jump and clear flag.
		if(WaitingInt & MMU::VBlank)
			exec_interrupt(MMU::VBlank, 0x0040);
		else if(WaitingInt & MMU::LCDSTAT)
			exec_interrupt(MMU::LCDSTAT, 0x0048);
		else if(WaitingInt & MMU::TimerOverflow)
			exec_interrupt(MMU::TimerOverflow, 0x0050);
		else if(WaitingInt & MMU::TransferComplete)
			exec_interrupt(MMU::TransferComplete, 0x0058);
		else if(WaitingInt & MMU::Transition)
			exec_interrupt(MMU::Transition, 0x0060);
	}
}

inline void LR35902::add_cycles(unsigned int c)
{
	_clock_instr_cycles += c;
	frame_cycles += double_speed() ? c / 2 : c;
}

inline void LR35902::push(addr_t addr)
{
	write(--_sp, word_t((addr >> 8) & 0xFF));
	write(--_sp, word_t(addr & 0xFF));
	assert(_sp <= 0xFFFF - 2);
}

inline addr_t LR35902::pop()
{
	assert(_sp <= 0xFFFF - 2);
	addr_t addr = 0;
	addr = read(_sp++);
	addr |= static_cast<addr_t>(read(_sp++)) << 8;
	return addr;
}
