#pragma once

#include <unordered_set>

#include "MMU.hpp"

/**
 * Zilog 80 CPU
 *
 * Gameboy CPU (Sharp LR35902 ?)
**/
class Z80
{
public:
	using word_t = uint8_t;
	using addr_t = uint16_t;
	
	static constexpr unsigned int	ClockRate = 4194304; // Hz
	
	enum Flag : word_t
	{
		Zero = 0x80,		///< Last result was 0x00
		Negative = 0x40,	///< Last operation was a substraction
		HalfCarry = 0x20,	///< Last result was > 0x0F
		Carry = 0x10		///< Last result was > 0xFF or < 0x00
	};

	MMU*	mmu = nullptr;
	
	Z80();
	~Z80() =default;
	
	/// Reset the CPU, zero-ing all registers. (Power-up state)
	void reset();
	
	/// Reset to post internal checks
	void reset_cart();
	
	/// Loads a BIOS and sets the PC to its first instruction
	bool loadBIOS(const std::string& path);
	
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
	
	void display_state()
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
	
	word_t	_ime; // Interrupt Master Enable
	
	// 8/16bits registers access helpers
	inline void set_hl(addr_t val) { _h = static_cast<word_t>((val >> 8) & 0xFF); _l = val & 0xFF; } 
	inline void set_de(addr_t val) { _d = (val >> 8) & 0xFF; _e = val & 0xFF; } 
	inline void set_bc(addr_t val) { _b = (val >> 8) & 0xFF; _c = val & 0xFF; } 
	inline void set_af(addr_t val) { _a = (val >> 8) & 0xFF; _f = val & 0xFF; } 
	inline word_t& fetch_hl() { add_cycles(1); return rw(getHL()); } // @todo replace add_cycles by read ?
	inline word_t& fetch_reg(word_t n) { return (n > 6) ? fetch_hl() : _r[n]; }
	inline void incr_hl() { _l++; if(_l == 0) _h++; }
	inline void decr_hl() { if(_l == 0) _h--; _l--; }
	
	// Flags helpers
	/// Sets (or clears if false is passed as second argument) the specified flag.
	inline void set(Flag m, bool b = true) { _f = b ? (_f | m) : (_f & ~m); }
	
	inline word_t& rw(addr_t addr) { return mmu->rw(addr); }
	inline word_t read(addr_t addr) const { return mmu->read(addr); }
	
	///////////////////////////////////////////////////////////////////////////
	// Interrupts management
	
	inline void exec_interrupt(MMU::InterruptFlag i, addr_t addr)
	{
		push(_pc);
		_pc = addr;
		_ime = 0x00;
		add_cycles(5);
		mmu->rw(MMU::IF) &= ~i;
	}
	
	inline void check_interrupts()
	{
		if(_ime == 0x00)
			return;
		
		word_t IF = read(MMU::IF);
		if(IF)
		{
			word_t IE = read(MMU::IE);
			// Check each interrupt. If requested and enabled, push current PC, jump and clear flag.
			if((IF & MMU::VBlank) & (IE & MMU::VBlank)) {
				exec_interrupt(MMU::VBlank, 0x0040);
			} else if((IF & MMU::LCDSTAT) & (IE & MMU::LCDSTAT)) {
				exec_interrupt(MMU::LCDSTAT, 0x0048);
			} else if((IF & MMU::TimerOverflow) & (IE & MMU::TimerOverflow)) {
				exec_interrupt(MMU::TimerOverflow, 0x0050);
			} else if((IF & MMU::TransferComplete) & (IE & MMU::TransferComplete)) {
				exec_interrupt(MMU::TransferComplete, 0x0058);
			} else if((IF & MMU::Transition) & (IE & MMU::Transition)) {
				exec_interrupt(MMU::Transition, 0x0060);
			}
		}
	}
	
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
	// Instructions
	
	// Helper function on opcodes
	inline word_t extract_src_reg(word_t opcode) const { return (opcode + 1) & 0b111; }
	inline word_t extract_dst_reg(word_t opcode) const { return ((opcode >> 3) + 1) & 0b111; }
	inline void rel_jump(word_t offset) { _pc += offset - ((offset & 0b10000000) ? 0x100 : 0);  add_cycles(1); }
	
	#include "Z80Instr.inl"
};
