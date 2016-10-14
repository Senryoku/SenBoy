#include <iostream>
#include <chrono>

#include <Core/GameBoy.hpp>

std::string rom_path("tests/cpu_instrs/cpu_instrs.gb");

// Components of the emulated GameBoy
Cartridge	cartridge;
MMU			mmu(cartridge);
Gb_Apu		apu;
LR35902		cpu(mmu, apu);
GPU			gpu(mmu);

// Timing
std::chrono::high_resolution_clock timing_clock;
	
int main()
{
	std::cout << "Using '" << rom_path << "'.\n";

	apu.reset();
	cpu.reset();
	mmu.reset();
	if(!cartridge.load(rom_path))
	{
		std::cerr << "Error loading test ROM.\n";
		return 1;
	}
	cpu.reset_cart();
	gpu.reset();
	
	auto start = timing_clock.now();
	while(cpu.get_pc() != 0x06F1)
		cpu.execute();
	auto end = timing_clock.now();
	
	std::chrono::duration<double> diff = end - start;
	std::cout << "Time: " << diff.count() * 1000 << "ms." << std::endl;
	return diff.count() * 1000;
}
