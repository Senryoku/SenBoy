#pragma once

#include <unordered_set>

#include "gb_apu/Gb_Apu.h"

#include "MMU.hpp"

/**
 * Gameboy CPU (Sharp LR35902)
**/
class LR35902
{
public:
	static constexpr unsigned int	ClockRate = 4194304; // Hz
	
	enum Flag : word_t
	{
		Zero = 0x80,		///< Last result was 0x00
		Negative = 0x40,	///< Last operation was a substraction
		HalfCarry = 0x20,	///< Last result was > 0x0F
		Carry = 0x10		///< Last result was > 0xFF or < 0x00
	};

	MMU*	mmu = nullptr;
	Gb_Apu*	apu = nullptr;

	size_t  frame_cycles = 0;
	bool 	emulate_halt_bug = false; ///< @todo Debug
	
	LR35902();
	~LR35902() =default;
	
	/// Reset the CPU, zero-ing all registers. (Power-up state)
	void reset();
	
	/// Reset to post internal checks
	void reset_cart();
	
	inline bool double_speed() const { return (mmu->read(MMU::KEY1) & 0x80); }
	inline uint64_t getClockCycles() const { return _clock_cycles; }
	inline uint64_t getInstrCycles() const { return _clock_instr_cycles; }
	inline addr_t getPC() const { return _pc; }
	inline addr_t getSP() const { return _sp; }
	inline addr_t getAF() const { return (static_cast<addr_t>(_a) << 8) + _f; }
	inline addr_t getBC() const { return (static_cast<addr_t>(_b) << 8) + _c; }
	inline addr_t getDE() const { return (static_cast<addr_t>(_d) << 8) + _e; }
	inline addr_t getHL() const { return (static_cast<addr_t>(_h) << 8) + _l; }
	
    inline int getNextOpcode() const { return read(_pc); }
    inline int getNextOperand0() const { return read(_pc + 1); }
    inline int getNextOperand1() const { return read(_pc + 2); }
	
    inline bool reached_breakpoint() const { return _breakpoint;  }
    inline void add_breakpoint(addr_t addr) { _breakpoints.insert(addr); }
    inline void clear_breakpoints() { _breakpoints.clear(); }
	
	inline void display_state()
	{
		/*
		std::cout << "PC: 0x" << std::hex << (int) (_pc - 1) << ", Excuting opcode: 0x" << std::hex << (int) read(_pc - 1) 
			<< " [0x" << (int) read(_pc) << " - 0x" << (int) read(_pc + 1) << "] AF:0x" << (int) getAF() << " BC:0x" << (int) getBC() 
				<< " DE:0x" << (int) getDE() << " HL:0x" << (int) getHL() << std::endl;
		*/
	}
	
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
	
	bool	_ime = true; // Interrupt Master Enable
	
	bool	_stop = false;	// instr_stop
	bool	_halt = false;	// instr_halt
	
	// 8/16bits registers access helpers
	inline void set_hl(addr_t val) { _h = (val >> 8) & 0xFF; _l = val & 0xFF; } 
	inline void set_de(addr_t val) { _d = (val >> 8) & 0xFF; _e = val & 0xFF; } 
	inline void set_bc(addr_t val) { _b = (val >> 8) & 0xFF; _c = val & 0xFF; } 
	inline void set_af(addr_t val) { _a = (val >> 8) & 0xFF; _f = val & 0xF0; } // Low nibble of F is always 0!
		
	inline word_t fetch_hl_val() { return read(getHL()); }
	inline word_t fetch_val(word_t n) { return (n > 6) ? fetch_hl_val() : _r[n]; }
	
	inline void incr_hl() { _l++; if(_l == 0) _h++; }
	inline void decr_hl() { if(_l == 0) _h--; _l--; }
	
	// Flags helpers
	/// Sets (or clears if false is passed as second argument) the specified flag.
	inline void set(Flag m, bool b = true) { _f = b ? (_f | m) : (_f & ~m); }
	inline void set(word_t m, bool b = true) { _f = b ? (_f | m) : (_f & ~m); }
	
	// APU Intercept
	inline word_t read(addr_t addr) const
	{
		if(apu && Gb_Apu::start_addr <= addr && addr <= Gb_Apu::end_addr)
			return apu->read_register(double_speed() ? frame_cycles / 2 : frame_cycles, addr);
		else
			return mmu->read(addr);
	}
	
	inline void write(addr_t addr, word_t value)
	{
		if(apu && Gb_Apu::start_addr <= addr && addr <= Gb_Apu::end_addr)
			apu->write_register(double_speed() ? frame_cycles / 2 : frame_cycles, addr, value);
		else
			mmu->write(addr, value);
	}
	
	///////////////////////////////////////////////////////////////////////////
	// Interrupts management
	
	inline void exec_interrupt(MMU::InterruptFlag i, addr_t addr)
	{
		push(_pc);
		_pc = addr;
		_ime = false;
		add_cycles(20);
		mmu->rw(MMU::IF) &= ~i;
		_halt = false;
	}
	
	
	inline void update_timing()
	{
		// Updates at 16384Hz, which is ClockRate / 256.
		_divider_register += _clock_instr_cycles;
		if(_divider_register >= 256)
		{
			_divider_register -= 256;
			mmu->rw(MMU::DIV)++;
		}
			
		_timer_counter += _clock_instr_cycles;
		word_t TAC = mmu->read(MMU::TAC);
		unsigned int tac_divisor = 1026;
		if((TAC & 0b11) == 0b01) tac_divisor = 16;
		if((TAC & 0b11) == 0b10) tac_divisor = 64;
		if((TAC & 0b11) == 0b11) tac_divisor = 256;
		while((TAC & 0b100) && _timer_counter >= tac_divisor)
		{
			_timer_counter -= tac_divisor;
			if(mmu->read(MMU::TIMA) != 0xFF)
			{
				mmu->rw(MMU::TIMA)++;
			} else {
				mmu->rw(MMU::TIMA) = mmu->read(MMU::TMA);
				// Interrupt
				mmu->rw(MMU::IF) |= MMU::TimerOverflow;
			}
		}
	}
	
	inline void check_interrupts()
	{
		word_t IF = mmu->read(MMU::IF);
		word_t IE = mmu->read(MMU::IE);
		if(IF & IE) // An enabled interrupt is waiting
		{
			// Check each interrupt in order of priority.
			// If requested and enabled, push current PC, jump and clear flag.
			if(IF & MMU::VBlank) {
				exec_interrupt(MMU::VBlank, 0x0040);
			} else if(IF & MMU::LCDSTAT) {
				exec_interrupt(MMU::LCDSTAT, 0x0048);
			} else if(IF & MMU::TimerOverflow) {
				exec_interrupt(MMU::TimerOverflow, 0x0050);
			} else if(IF & MMU::TransferComplete) {
				exec_interrupt(MMU::TransferComplete, 0x0058);
			} else if(IF & MMU::Transition) {
				exec_interrupt(MMU::Transition, 0x0060);
			}
		}
	}
	
	///////////////////////////////////////////////////////////////////////////
	// Cycles management
	
	unsigned int	_clock_cycles = 0;			///< Clock cycles since reset
	unsigned int	_clock_instr_cycles = 0;	///< Clock cycles of the last instruction
	unsigned int	_divider_register = 0;		///< Cycles not yet counted in DIV
	unsigned int	_timer_counter = 0;			///< Cycles not yet counted in TIMA
	
	inline void add_cycles(unsigned int c)
	{
		_clock_instr_cycles += c;
		frame_cycles += c;
	}
	
	///////////////////////////////////////////////////////////////////////////
	// Stack management
	
	inline void push(addr_t addr)
	{
		write(--_sp, word_t((addr >> 8) & 0xFF));
		write(--_sp, word_t(addr & 0xFF));
		assert(_sp <= 0xFFFF - 2);
	}
	
	inline addr_t pop()
	{
		assert(_sp <= 0xFFFF - 2);
		addr_t addr = 0;
		addr = read(_sp++);
		addr |= static_cast<addr_t>(read(_sp++)) << 8;
		return addr;
	}
	
	///////////////////////////////////////////////////////////////////////////
	// Instructions
	
	// Helper functions on opcodes
	inline word_t extract_src_reg(word_t opcode) const { return (opcode + 1) & 0b111; }
	inline word_t extract_dst_reg(word_t opcode) const { return ((opcode >> 3) + 1) & 0b111; }
	inline void rel_jump(word_t offset) { _pc += from_2c_to_signed(offset); }
	
	inline int from_2c_to_signed(word_t src)
	{
		return (src & 0x80) ? -((~src + 1) & 0xFF) : src;
	}
	
	#include "LR35902Instr.inl"
};
