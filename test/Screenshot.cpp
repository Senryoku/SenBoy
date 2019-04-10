#include <iostream>
#include <Core/GameBoy.hpp>
#include <miniz.h>
#include <experimental/filesystem>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "gif.h"

std::string rom_path("D:/Source/SenBoy/gbc/Rayman (Europe) (En,Fr,De,Es,It,Nl).gbc");

// Components of the emulated GameBoy
Cartridge	cartridge;
MMU			mmu(cartridge);
Gb_Apu		apu;
LR35902		cpu(mmu, apu);
GPU			gpu(mmu);

int main(int argc, char* argv[])
{
	if(argc > 1)
		rom_path = argv[1];
	//std::cout << "Using '" << rom_path << "'." << std::endl;
	
	std::string filename;
	
	auto period_pos = rom_path.find_last_of('.');
	if(period_pos == std::string::npos)
		period_pos = rom_path.size();
	auto path_pos = rom_path.find_last_of('/');
	if(path_pos == std::string::npos)
		path_pos = rom_path.find_last_of('\\');
	if(path_pos != std::string::npos)
		filename = rom_path.substr(path_pos + 1, period_pos - path_pos - 1);
	else 
		return 1;
	
	filename = std::string{"GBScreenshots/"} + filename;
	/*
	if(std::experimental::filesystem::exists(filename))
		return 0;
	*/
	//cartridge.Log = [](const std::string& s) { std::cout << " > " << s << std::endl; };
	
	apu.reset();
	cpu.reset();
	mmu.reset();
	mmu.callback_joy_a = 		[&] () -> bool { return 0 & 0x01; };
	mmu.callback_joy_b = 		[&] () -> bool { return 0 & 0x02; };
	mmu.callback_joy_select = 	[&] () -> bool { return 0 & 0x04; };
	mmu.callback_joy_start = 	[&] () -> bool { return 0 & 0x08; };
	mmu.callback_joy_right = 	[&] () -> bool { return 0 & 0x10; };
	mmu.callback_joy_left = 	[&] () -> bool { return 0 & 0x20; };
	mmu.callback_joy_up = 		[&] () -> bool { return 0 & 0x40; };
	mmu.callback_joy_down = 	[&] () -> bool { return 0 & 0x80; };
	
	std::string ext;
	if(period_pos != std::string::npos)
		ext = rom_path.substr(rom_path.find_last_of('.') + 1, rom_path.size());
	// Seems to be a zip archive
	if(period_pos != std::string::npos && ext == "zip") {
		mz_zip_archive zip_archive;
		memset(&zip_archive, 0, sizeof(zip_archive));
		auto status = mz_zip_reader_init_file(&zip_archive, rom_path.c_str(), 0);
		if (!status)
		{
			return 1;
		} else {
			bool loaded = false;
			// Search the files in the archive for a suitable ROM
			for(int i = 0; i < static_cast<int>(mz_zip_reader_get_num_files(&zip_archive)); ++i)
			{
				mz_zip_archive_file_stat file_stat;
				// Get file name
				if(!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
					continue;
				std::string filename{file_stat.m_filename};
				auto period_pos = filename.find_last_of('.');
				std::string ext;
				if(period_pos != std::string::npos)
					ext = filename.substr(filename.find_last_of('.') + 1, filename.size());
				if(ext == "gb" || ext == "gbc")
				{
					std::vector<char> buffer(file_stat.m_uncomp_size);
					if(!mz_zip_reader_extract_file_to_mem(&zip_archive, file_stat.m_filename, buffer.data(), file_stat.m_uncomp_size, 0))
					{
						return 1;
					} else {
						if(!cartridge.load_from_memory(reinterpret_cast<unsigned char*>(buffer.data()), buffer.size())) 
						{
							return 1;
						} else {
							loaded = true;
							break;
						}
					}
				}
			}
			if(!loaded)
				return 1;
		}
		mz_zip_reader_end(&zip_archive);
	} else { // Directly load the file into memory
		if(!cartridge.load(rom_path))
		{
			std::cerr << "Error loading test ROM." << std::endl;
			return 1;
		}
	}
	cpu.reset_cart();
	//mmu.load_boot();
	//mmu.load_senboot();
	gpu.reset();
	
	auto directory = filename + std::string{"/"};
	
	const bool save_video = true;
	
	try {
		if(!std::experimental::filesystem::exists(directory))
			std::experimental::filesystem::create_directory(directory);
	} catch(...) {
	}
	
	GifWriter gif;
	GifBegin(&gif, (filename + std::string{".gif"}).c_str(), gpu.ScreenWidth, gpu.ScreenHeight, 2);
	
	const int shot_count = 1;
	const int shot_period = 30;

	for(int shot = 0; shot < shot_count; ++shot) {
		for(int i = 0; i < 60 * shot_period; ++i) {
			//std::cout << "Frame " << i << std::endl;
			while(cpu.frame_cycles <= 70224) {
				cpu.execute();
				if(cpu.get_instr_cycles() == 0)
					break;
				size_t instr_cycles = (cpu.double_speed() ? cpu.get_instr_cycles() / 2 :
												cpu.get_instr_cycles());
				gpu.step(instr_cycles, save_video);
			} 
			
			if(save_video) {
				/*
				auto count = std::to_string(i);
				count = std::string(4 - count.length(), '0') + count;
				if(stbi_write_png((directory + count + std::string{".png"}).c_str(), gpu.ScreenWidth, gpu.ScreenHeight, 4, gpu.get_screen(), 4 * gpu.ScreenWidth) == 0)
					std::cerr << "stbi_write_png error" << std::endl;
				*/
				GifWriteFrame(&gif, reinterpret_cast<const uint8_t*>(gpu.get_screen()), gpu.ScreenWidth, gpu.ScreenHeight, 2);
			}

			size_t frame_cycles = cpu.frame_cycles;
			apu.end_frame(frame_cycles);
			cpu.frame_cycles = 0;
			
			//std::cout << "\rFrame " << i + 1 << " / " << 60 * shot_period;
		}
		
		if(shot != shot_count - 1) {
			while(cpu.frame_cycles <= 70224) {
				cpu.execute();
				if(cpu.get_instr_cycles() == 0)
					break;
				size_t instr_cycles = (cpu.double_speed() ? cpu.get_instr_cycles() / 2 :
												cpu.get_instr_cycles());
				gpu.step(instr_cycles);
			}
			
			auto count = std::to_string(shot);
			count = std::string(4 - count.length(), '0') + count;
			if(stbi_write_png((directory + std::string{'_'} + count + std::string{".png"}).c_str(), gpu.ScreenWidth, gpu.ScreenHeight, 4, gpu.get_screen(), 4 * gpu.ScreenWidth) == 0)
				std::cerr << "stbi_write_png error" << std::endl;
			std::cout << std::endl;
		}
	}
	
	if(stbi_write_png((filename + std::string{".png"}).c_str(), gpu.ScreenWidth, gpu.ScreenHeight, 4, gpu.get_screen(), 4 * gpu.ScreenWidth) == 0)
		std::cerr << "stbi_write_png error" << std::endl;
	std::cout << std::endl;
	
	GifEnd(&gif);
}
