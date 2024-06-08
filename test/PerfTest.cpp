#include <chrono>
#include <iostream>

#include <Core/GameBoy.hpp>

std::string rom_path("gbc/Pokemon - Crystal Version (USA, Europe).gbc");

// Components of the emulated GameBoy
Cartridge cartridge;
MMU       mmu(cartridge);
Gb_Apu    apu;
LR35902   cpu(mmu, apu);
GPU       gpu(mmu);

// Timing
std::chrono::high_resolution_clock timing_clock;

int main() {
    std::cout << "Using '" << rom_path << "'.\n";

    auto no_input = []() -> bool { return false; };
    mmu.callback_joy_a = mmu.callback_joy_b = mmu.callback_joy_select = mmu.callback_joy_start = mmu.callback_joy_right = mmu.callback_joy_left = mmu.callback_joy_up =
        mmu.callback_joy_down = no_input;

    apu.reset();
    cpu.reset();
    mmu.reset();
    if(!cartridge.load(rom_path)) {
        std::cerr << "Error loading test ROM.\n";
        return 1;
    }
    cpu.reset_cart();
    gpu.reset();

    size_t frame_count = 0x800000;
    auto   start = timing_clock.now();
    for(size_t i = 0; i < frame_count; ++i) {
        cpu.execute();
        size_t instr_cycles = (cpu.double_speed() ? cpu.get_instr_cycles() / 2 : cpu.get_instr_cycles());
        gpu.step(instr_cycles);
    }
    auto end = timing_clock.now();

    auto diff = end - start;
    std::cout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()
              << "ms. (Average frame time: " << std::chrono::duration_cast<std::chrono::microseconds>(diff).count() / frame_count << "us)" << std::endl;
    return diff.count() * 1000;
}
