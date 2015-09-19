#include "Z80.hpp"

int main(int argc, char* argv[])
{
	Z80 cpu;
	ROM rom("tests/cpu_instrs/cpu_instrs.gb");
	
	cpu.rom = &rom;
	cpu.reset();
	cpu.reset_cart();
	int i = 1000;
	while(i-- > 0)
		cpu.execute();
}