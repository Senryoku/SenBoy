#pragma once

#include <limits>
#include <vector>
#include <set>

#include <Core/Cartridge.hpp>
#include <Core/LR35902.hpp>
#include <Tools/Hexadecimal.hpp>

class Analyser
{
public:
	struct Label
	{
		unsigned int	id = std::numeric_limits<unsigned int>::max();
		std::string		name;
		addr_t			addr;
		
		operator bool() const
		{
			return id != std::numeric_limits<unsigned int>::max();
		}
	};
	
	void process(const Cartridge& c)
	{
		clear();
		labels.resize(c.getROMSize());
		process(c, 0x0040);
		process(c, 0x0048);
		process(c, 0x0050);
		process(c, 0x0058);
		process(c, 0x0060);
		process(c, 0x0100);
	}
	
	void process(const Cartridge& c, addr_t addr)
	{
		while(addr < c.getROMSize())
		{
			if(instructions.count(addr) > 0)
				return;
			
			word_t opcode = c.read(addr);

			if(LR35902::instr_length[opcode] == 0)
			{
				if(opcode == 0xCB)
				{
					instructions.insert(addr);
					addr += 2;
					continue;
				} else {
					break;
				}
			}

			instructions.insert(addr);
			
			auto jump = [&] (addr_t jp_addr) {
				if(!labels[jp_addr])
				{
					add_label(jp_addr);
					process(c, jp_addr);
				}
			};
			
			// TODO: Do something with rets? (marks 'functions')
			switch(opcode)
			{
				case 0x20: //[[fallthrough]] // JR NZ,r8;	Conditional (non-zero) relative jump
				case 0x30: //[[fallthrough]] // JR NC,r8;	Conditional (non-carry) relative jump
				case 0x28: //[[fallthrough]] // JR Z,r8;	Conditional (zero) relative jump
				case 0x38: //[[fallthrough]] // JR C,r8;	Conditional (carry) relative jump
				case 0xCD: 					 // Call a16;	Unconditional absolute jump, but will return after!
				{
					addr_t jp_addr = addr + from_2c_to_signed(c.read(addr + 1));
					jump(jp_addr);
					break;
				}
				case 0xC2: // JP NZ,a16;	Conditional (non-zero) absolute jump
				case 0xD2: // JP NC,a16;	Conditional (non-carry) absolute jump
				case 0xC4: // CALL NZ,a16;	Conditional (non-zero) absolute jump
				case 0xD4: // CALL NC,a16;	Conditional (non-carry) absolute jump
				case 0xCC: // CALL Z,a16;	Conditional (carry) absolute jump
				case 0xDC: // CALL C,a16;	Conditional (carry) absolute jump
				{
					addr_t jp_addr = read16(c, addr + 1);
					jump(jp_addr);
					break;
				}
				case 0x18: // JR r8;		Unconditional relative jump
				{
					addr_t jp_addr = addr + from_2c_to_signed(c.read(addr + 1));
					jump(jp_addr);
					return; // Unconditional => Next instruction can't be reach this way.
				}
				case 0xC3: // JP a16;		Unconditional absolute jump
				{
					addr_t jp_addr = read16(c, addr + 1);
					jump(jp_addr);
					return; // Unconditional => Next instruction can't be reach this way.
				}
				// RST 00H, 08H, 10H, 18H, 20H, 28H, 30H, 38H; Unconditional absolute jumps, but will return after!
				case 0xC7: jump(0x0000); break;
				case 0xCF: jump(0x0008); break;
				case 0xD7: jump(0x0010); break;
				case 0xDF: jump(0x0018); break;
				case 0xE7: jump(0x0020); break;
				case 0xEF: jump(0x0028); break;
				case 0xF7: jump(0x0030); break;
				case 0xFF: jump(0x0038); break;
				// Unconditional returns
				case 0xC9: // RET
				case 0xD9: // RETI
					return;
				case 0xE9: // JP (HL); Unconditional, 'runtime' jump don't know what to do with it for now (TODO)
					return;
				// RET NZ (0xC0), RET NC (0xD0), RET Z (0xC8), RET C (0xD8 Conditional returns, nothing to do.
				default:
					break;
			}
			
			addr += LR35902::instr_length[opcode];
		}
	}
	
	void clear()
	{
		_next_id = 0;
		labels.clear();
		instructions.clear();
	}
	
	unsigned int get_label_count() const 
	{
		return _next_id;
	}

	std::vector<Label>	labels;
	std::set<addr_t>	instructions;

private:
	unsigned int _next_id = 0;

	void add_label(addr_t addr)
	{
		auto id_str = std::to_string(_next_id);
		std::string label = "L" + std::string(3 - id_str.length(), '0') + id_str;
		labels[addr] = {_next_id, label, addr};
		++_next_id;
	}
	
	static inline addr_t read16(const Cartridge& c, addr_t addr)
	{
		return ((static_cast<addr_t>(c.read(addr + 1)) << 8) & 0xFF00) | c.read(addr);
	}
	
	static inline int from_2c_to_signed(word_t src) { return (src & 0x80) ? -((~src + 1) & 0xFF) : src; }
};
