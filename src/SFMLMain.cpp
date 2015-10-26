#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include "MMU.hpp"
#include "Cartridge.hpp"
#include "GPU.hpp"
#include "LR35902.hpp"
#include "GBAudioStream.hpp"

#include "CommandLine.hpp"
#include "Config.hpp"

// Options
bool debug_display = false;
bool use_bios = true;
bool with_sound = true;
bool debug = false;			// Pause execution
bool step = false;			// Step to next instruction (in debug)
bool frame_by_frame = true;
bool real_speed = true;
size_t sample_rate = 44100;	// Audio sample rate
size_t frame_skip = 0;		// Increase for better performances.

// Components of the emulated GameBoy
Cartridge	cartridge;
LR35902		cpu;
Gb_Apu		apu;
MMU			mmu;
GPU			gpu;

Stereo_Buffer gb_snd_buffer;
GBAudioStream snd_buffer;

// GUI
float screen_scale = 3.0f;
size_t padding = 25 * 2 * screen_scale;
sf::RenderWindow window;
sf::RenderWindow window_debug;
sf::Texture	gameboy_screen;
sf::Sprite gameboy_screen_sprite;
sf::Texture	gameboy_logo_tex;
sf::Sprite gameboy_logo_sprite;

// Debug GUI
sf::Texture	gameboy_tiledata[2];
sf::Sprite gameboy_tiledata_sprite[2];
sf::Texture	gameboy_tilemap[2];
sf::Sprite gameboy_tilemap_sprite[2];
sf::Font font;
sf::Text debug_text;
sf::Text log_text;
sf::Text tiledata_text[2];
color_t* tile_data[2] = {nullptr, nullptr};
color_t* tile_maps[2] = {nullptr, nullptr};

// Timing
sf::Clock timing_clock;
double frame_time = 0;
size_t speed_update = 10;
double speed = 100;
uint64_t elapsed_cycles = 0;
uint64_t speed_mesure_cycles = 0;
size_t frame_count = 0;
	
// Movie Playback (.vbm)
// Not in sync. Doesn't work.
bool use_movie = false;
std::ifstream movie;
std::ofstream movie_save;
unsigned int movie_start = 0;
char playback[2];
enum MovieType
{
	VBM,
	BK2,
	SBM		// SenBoy Movie
};
MovieType	movie_type = VBM;

void open_debug_window();
void setup_window();
void setup_debug_window();
void handle_event(sf::Event event);
void handle_event_debug(sf::Event event);
std::string get_debug_text();
std::string get_debug_log();
void toggle_speed();
void reset();
void get_input_from_movie();
void movie_save_frame();
void update_tiledata();
void update_tilemaps();

int main(int argc, char* argv[])
{
	config::set_folder(argv[0]);
	
	if(argc == 1 || has_option(argc, argv, "-h"))
	{
		std::cout << "SenBoy - Usage:" << std::endl
				<< "./SenBoy \"path/to/rom\" [-d] [-b]" << std::endl
				<< " -d : Enable debug display." << std::endl
				<< " -b : Skip BIOS." << std::endl
				<< " -s : Disable sound." << std::endl
				<< " $m : Specify a input movie file." << std::endl
				<< " $ms : Specify a output movie file." << std::endl
				<< " --dmg : Force DMG mode." << std::endl
				<< " --cgb : Force CGB mode." << std::endl;
		if(has_option(argc, argv, "-h")) return 0;
	}
	
	std::string path(config::to_abs("tests/cpu_instrs/cpu_instrs.gb"));
	char* rom_path = get_file(argc, argv);
	if(rom_path)
		path = rom_path;
	
	if(has_option(argc, argv, "-d"))
		debug = debug_display = true;
	if(has_option(argc, argv, "-b"))
		use_bios = false;
	if(has_option(argc, argv, "-s"))
		with_sound = false;
	if(has_option(argc, argv, "--dmg"))
		mmu.force_dmg = true;
	if(has_option(argc, argv, "--cgb"))
		mmu.force_cgb = true;
	
	// Linking them all together
	mmu.cartridge = &cartridge;
	if(with_sound) cpu.apu = &apu;
	cpu.mmu = &mmu;
	gpu.mmu = &mmu;
	
	// Loading ROM
	if(!cartridge.load(path))
		return 0;
	
	// Audio buffers
	gb_snd_buffer.clock_rate(LR35902::ClockRate);
	gb_snd_buffer.set_sample_rate(sample_rate);
	apu.output(gb_snd_buffer.center(), gb_snd_buffer.left(), gb_snd_buffer.right());
	snd_buffer.setVolume(50);

	// Movie Saving
	char* movie_save_path = get_option(argc, argv, "$ms");
	if(movie_save_path)
	{
		movie_save.open(movie_save_path, std::ios::binary);
	}
	
	// Movie loading
	char* movie_path = get_option(argc, argv, "$m");
	if(movie_path)
	{
		switch(movie_type)
		{
			case SBM:
			{
				movie.open(movie_path, std::ios::binary);
				if(!movie) {
					std::cerr << "Error: Unable to open movie file '" << movie_path << "'." << std::endl;
				} else {
					movie_start = movie.tellg();
					use_movie = true;
					use_bios = false;
					std::cout << "Loaded SenBoy Movie file '" << movie_path << "'. Starting in playback mode..." << std::endl;
				}
				break;
			}
			case VBM:
				movie.open(movie_path, std::ios::binary);
				if(!movie) {
					std::cerr << "Error: Unable to open movie file '" << movie_path << "'." << std::endl;
				} else {
					// VBM
					char tmp[4];
					movie.seekg(0x03C); // Start of the inputs, 4bytes unsigned int little endian
					movie.read(tmp, 4);
					movie_start = tmp[0] | (tmp[1] << 8) | (tmp[2] << 16) | (tmp[3] << 24);
					movie.seekg(movie_start);
					use_movie = true;
					use_bios = false;
					
					std::cout << "Loaded movie file '" << movie_path << "'. Starting in playback mode..." << std::endl;
				}
				break;
			case BK2: // BK2 (Not the archive, just the Input Log)
				movie.open(movie_path);
				if(!movie) {
					std::cerr << "Error: Unable to open movie file '" << movie_path << "'." << std::endl;
				} else {
					movie.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
					movie_start = movie.tellg();
					use_movie = true;
					use_bios = false;
					
					std::cout << "Loaded movie file '" << movie_path << "'. Starting in playback mode..." << std::endl;
				}
				break;
		}
	}
	
	// Input callbacks
	if(!use_movie)
	{
		mmu.callback_joy_up = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::Y) < -50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Up); };
		mmu.callback_joy_down = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::Y) > 50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Down); };
		mmu.callback_joy_right = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::X) > 50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Right); };
		mmu.callback_joy_left = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::X) < -50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Left); };
		mmu.callback_joy_a = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 0)) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::S); };
		mmu.callback_joy_b = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 1)) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::D); };
		mmu.callback_joy_select = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 6)) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::F); };
		mmu.callback_joy_start = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 7)) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::G); };
	} else {
		mmu.callback_joy_a = 		[&] () -> bool { return playback[0] & 0x01; };
		mmu.callback_joy_b = 		[&] () -> bool { return playback[0] & 0x02; };
		mmu.callback_joy_select = 	[&] () -> bool { return playback[0] & 0x04; };
		mmu.callback_joy_start = 	[&] () -> bool { return playback[0] & 0x08; };
		mmu.callback_joy_right = 	[&] () -> bool { return playback[0] & 0x10; };
		mmu.callback_joy_left = 	[&] () -> bool { return playback[0] & 0x20; };
		mmu.callback_joy_up = 		[&] () -> bool { return playback[0] & 0x40; };
		mmu.callback_joy_down = 	[&] () -> bool { return playback[0] & 0x80; };
	}
	
	///////////////////////////////////////////////////////////////////////////
	// Setting up GUI
	
	setup_window();
	if(debug_display) open_debug_window();
	setup_debug_window();

	///////////////////////////////////////////////////////////////////////////
	
	reset();
	
    sf::Event event;
	while (window.isOpen())
    {
        while(window.pollEvent(event))
			handle_event(event);
		
		double gameboy_time = double(elapsed_cycles) / cpu.ClockRate;
		double diff = gameboy_time - timing_clock.getElapsedTime().asSeconds();
		if(real_speed && !debug && diff > 0)
			sf::sleep(sf::seconds(diff));
		if(!debug || step)
		{
			cpu.frame_cycles = 0;
			for(size_t i = 0; i < frame_skip + 1; ++i)
			{
				get_input_from_movie();
				movie_save_frame();
				do
				{
					cpu.execute();
					if(cpu.get_instr_cycles() == 0)
						break;

					size_t instr_cycles = (cpu.double_speed() ? cpu.get_instr_cycles() / 2 :
																cpu.get_instr_cycles());
					gpu.step(instr_cycles, i == frame_skip);
					elapsed_cycles += instr_cycles;
					speed_mesure_cycles += instr_cycles;
			
					if(cpu.reached_breakpoint())
					{
						std::stringstream ss;
						ss << "Stepped on a breakpoint at " << Hexa(cpu.get_pc());
						log_text.setString(ss.str());
						debug = true;
						step = false;
						break;
					}
				} while((!debug || frame_by_frame) && (!gpu.completed_frame() || cpu.frame_cycles <= 70224));
				
				frame_count++;
			}
			
			if(with_sound)
			{
				size_t frame_cycles = cpu.frame_cycles;
				bool stereo = apu.end_frame(frame_cycles);
				gb_snd_buffer.end_frame(frame_cycles, stereo);
				size_t samples_count = gb_snd_buffer.samples_avail();
				if(samples_count > 0)
				{
					if(samples_count >= snd_buffer.buffer_size)
					{
						std::cout << "Warning: Way too many sound samples at once. Discarding some..." << std::endl;
						blip_sample_t discard[snd_buffer.chunk_size];
						while(samples_count >= snd_buffer.chunk_size)
						{
							gb_snd_buffer.read_samples(discard, snd_buffer.chunk_size);
							samples_count -= snd_buffer.chunk_size;
						}
					}
					samples_count = gb_snd_buffer.read_samples(snd_buffer.add_samples(samples_count), samples_count);
					if(!(snd_buffer.getStatus() == sf::SoundSource::Status::Playing)) snd_buffer.play();
				}
			}
			
			gameboy_screen.update(reinterpret_cast<uint8_t*>(gpu.screen));
			
			frame_by_frame = false;
			step = false;
		}
		
		if(debug)
		{
			window.setTitle("SenBoy - Paused");
		} else if(--speed_update == 0) {
			speed_update = 10;
			double t = timing_clock.getElapsedTime().asSeconds();
			speed = 100.0 * (double(speed_mesure_cycles) / cpu.ClockRate) / (t - frame_time);
			frame_time = t;
			speed_mesure_cycles = 0;
			std::stringstream dt;
			dt << "Speed " << std::dec << std::fixed << std::setw(4) << std::setprecision(1) << speed << "%";
			window.setTitle(std::string("SenBoy - ").append(dt.str()));
		}
		
		if(window_debug.isOpen())
		{
			while(window_debug.pollEvent(event))
				handle_event_debug(event);
			
			debug_text.setString(get_debug_text());
			log_text.setPosition(5, window_debug.getSize().y - log_text.getGlobalBounds().height - 8);
			
			update_tiledata();
			update_tilemaps();
			
			window_debug.clear(sf::Color::Black);
			window_debug.draw(debug_text);
			window_debug.draw(log_text);
			for(int i = 0; i < 2; ++i)
			{
				window_debug.draw(tiledata_text[i]);
				window_debug.draw(gameboy_tiledata_sprite[i]);
				window_debug.draw(gameboy_tilemap_sprite[i]);
			}
			window_debug.display();
		}
		
        window.clear(sf::Color::Black);
		window.draw(gameboy_screen_sprite);
		window.draw(gameboy_logo_sprite);
        window.display();
    }
	
	cartridge.save();
	
	delete[] tile_data[0];
	delete[] tile_data[1];
	delete[] tile_maps[0];
	delete[] tile_maps[1];
}

void open_debug_window()
{
	if(window_debug.isOpen())
		return;
	window_debug.create(sf::VideoMode(1.25 * padding + 2 * 0.5 * screen_scale * (16 * 8 + 32 * 8), 
		screen_scale * 0.5 * 32 * 8 + padding), 
		"SenBoy - Debug");
}

void toggle_debug()
{
	debug = !debug;
	elapsed_cycles = 0;
	timing_clock.restart();
	log_text.setString(debug ? "Debugging" : "Running");
}

void add_breakpoint()
{
	addr_t addr;
	std::cout << "Adding breakpoint. Enter an address: " << std::endl;
	std::cin >> std::hex >> addr;
	cpu.add_breakpoint(addr);
	std::stringstream ss;
	ss << "Added a breakpoint at " << Hexa(addr) << ".";
	log_text.setString(ss.str());
}

void clear_breakpoints()
{
	cpu.clear_breakpoints();
	log_text.setString("Cleared breakpoints.");
}

void advance_frame()
{
	frame_by_frame = true;
	step = true;
	debug = true;
	log_text.setString("Advancing one frame");
}

void handle_event(sf::Event event)
{
	if (event.type == sf::Event::Closed)
		window.close();
	if (event.type == sf::Event::KeyPressed)
	{
		switch(event.key.code)
		{
			case sf::Keyboard::Escape: window.close(); break;
			case sf::Keyboard::Space: step = true; break;
			case sf::Keyboard::Return: toggle_debug(); break;
			case sf::Keyboard::BackSpace: reset(); break;
			case sf::Keyboard::Add:
			{
				float vol = snd_buffer.getVolume();
				vol = std::min(vol + 10.0f, 100.0f);
				snd_buffer.setVolume(vol);
				log_text.setString(std::string("Volume at ").append(std::to_string(vol)));
				break;
			}
			case sf::Keyboard::Subtract:
			{
				float vol = snd_buffer.getVolume();
				vol = std::max(vol - 10.0f, 0.0f);
				snd_buffer.setVolume(vol);
				log_text.setString(std::string("Volume at ").append(std::to_string(vol)));
				break;
			}
			case sf::Keyboard::D: open_debug_window(); break;
			case sf::Keyboard::B: add_breakpoint(); break;
			case sf::Keyboard::N: clear_breakpoints(); break;
			case sf::Keyboard::M: toggle_speed(); break;
			case sf::Keyboard::L: advance_frame(); break;
			default: break;
		}
	} else if (event.type == sf::Event::JoystickButtonPressed) { // Joypad Interrupt
		switch(event.joystickButton.button)
		{
			case 0: mmu.key_down(MMU::Button, MMU::RightA); break;
			case 2:
			case 1: mmu.key_down(MMU::Button, MMU::LeftB); break;
			case 6: mmu.key_down(MMU::Button, MMU::UpSelect); break;
			case 7: mmu.key_down(MMU::Button, MMU::DownStart); break;
			case 5: toggle_speed(); break;
			default: break;
		}
	} else if (event.type == sf::Event::JoystickButtonReleased) {
		switch(event.joystickButton.button)
		{
			case 5: toggle_speed(); break;
			default: break;
		}
	} else if (event.type == sf::Event::JoystickMoved) { // Joypad Interrupt
		if (event.joystickMove.axis == sf::Joystick::X)
		{
			if(event.joystickMove.position > 50) mmu.key_down(MMU::Direction, MMU::RightA);
			else if(event.joystickMove.position < -50) mmu.key_down(MMU::Direction, MMU::LeftB);
		} else if (event.joystickMove.axis == sf::Joystick::Y) {
			if(event.joystickMove.position > 50) mmu.key_down(MMU::Direction, MMU::UpSelect);
			else if(event.joystickMove.position < -50) mmu.key_down(MMU::Direction, MMU::DownStart);
		}
	}
}

void handle_event_debug(sf::Event event)
{
	if (event.type == sf::Event::Closed)
		window_debug.close();
	if (event.type == sf::Event::KeyPressed)
	{
		switch(event.key.code)
		{
			case sf::Keyboard::Escape: window_debug.close(); break;
			case sf::Keyboard::Space: step = true; break;
			case sf::Keyboard::Return: toggle_debug(); break;
			case sf::Keyboard::B: add_breakpoint(); break;
			case sf::Keyboard::N: clear_breakpoints(); break;
			case sf::Keyboard::M: toggle_speed(); break;
			case sf::Keyboard::L: advance_frame(); break;
			default: break;
		}
	}
}

void toggle_speed()
{
	real_speed = !real_speed;
	elapsed_cycles = 0;
	timing_clock.restart();
	log_text.setString(real_speed ? "Running at real speed" : "Running as fast as possible");
}

void reset()
{
	gpu.reset();
	cpu.reset();
	if(use_bios)
		mmu.load_bios();
	else
		cpu.reset_cart();
	elapsed_cycles = 0;
	timing_clock.restart();
	debug_text.setString("Reset");
	size_t frame_cycles = (cpu.double_speed() ? cpu.frame_cycles / 2 : cpu.frame_cycles);
	apu.end_frame(frame_cycles);
	if(use_movie) movie.seekg(movie_start);
}

// Get next input from the movie file.
void get_input_from_movie()
{
	if(use_movie)
	{
		switch(movie_type)
		{
			case SBM:
				movie.read(playback, 1);
				break;
			case VBM:
				movie.read(playback, 2);
				break;
			case BK2:
			{
				char line[13];
				movie.getline(line, 13);
				playback[0] = 0;
				if(line[1] != '.') playback[0] |= 0x40; // Up
				if(line[2] != '.') playback[0] |= 0x80; // Down
				if(line[3] != '.') playback[0] |= 0x20; // Left
				if(line[4] != '.') playback[0] |= 0x10; // Right
				if(line[5] != '.') playback[0] |= 0x08; // Start
				if(line[6] != '.') playback[0] |= 0x04; // Select
				if(line[7] != '.') playback[0] |= 0x02; // B
				if(line[8] != '.') playback[0] |= 0x01; // A
				break;
			}
		}
	}
}

void movie_save_frame()
{
	word_t input = 0;
	if(mmu.callback_joy_a()) 		input |= 0x01;
	if(mmu.callback_joy_b()) 		input |= 0x02;
	if(mmu.callback_joy_select()) 	input |= 0x04;
	if(mmu.callback_joy_start()) 	input |= 0x08;
	if(mmu.callback_joy_right()) 	input |= 0x10;
	if(mmu.callback_joy_left()) 	input |= 0x20;
	if(mmu.callback_joy_up()) 		input |= 0x40;
	if(mmu.callback_joy_down()) 	input |= 0x80;
	movie_save << input;
}

std::string get_debug_text()
{
	std::stringstream dt;
	dt << "PC: " << Hexa(cpu.get_pc());
	dt << " SP: " << Hexa(cpu.get_sp());
	dt << " | " << cpu.get_disassembly();
	dt << " | OP: " << Hexa8(cpu.get_next_opcode()) << " ";
	if(LR35902::instr_length[cpu.get_next_opcode()] > 1)
		dt << Hexa8(cpu.get_next_operand0()) << " ";
	if(LR35902::instr_length[cpu.get_next_opcode()] > 2)
		dt << Hexa8(cpu.get_next_operand1());
	dt << std::endl;
	dt << "AF: " << Hexa(cpu.get_af());
	dt << " BC: " << Hexa(cpu.get_bc());
	dt << " DE: " << Hexa(cpu.get_de());
	dt << " HL: " << Hexa(cpu.get_hl());
	dt << std::endl;
	dt << "LY: " << Hexa8(gpu.get_line());
	dt << " LCDC: " << Hexa8(gpu.get_lcdc());
	dt << " STAT: " << Hexa8(gpu.get_lcdstat());
	if(cpu.check(LR35902::Flag::Zero)) dt << " Z";
	if(cpu.check(LR35902::Flag::Negative)) dt << " N";
	if(cpu.check(LR35902::Flag::HalfCarry)) dt << " HC";
	if(cpu.check(LR35902::Flag::Carry)) dt << " C";
	return dt.str();
}

std::string get_debug_log()
{
	std::stringstream dt;
	dt << Hexa(cpu.get_pc());
	dt << " " << cpu.get_disassembly();
	dt << " " << Hexa8(cpu.get_next_opcode()) << " ";
	if(LR35902::instr_length[cpu.get_next_opcode()] > 1)
		dt << Hexa8(cpu.get_next_operand0()) << " ";
	if(LR35902::instr_length[cpu.get_next_opcode()] > 2)
		dt << Hexa8(cpu.get_next_operand1());
	dt << " SP: " << Hexa(cpu.get_sp());
	dt << " AF: " << Hexa(cpu.get_af());
	dt << " BC: " << Hexa(cpu.get_bc());
	dt << " DE: " << Hexa(cpu.get_de());
	dt << " HL: " << Hexa(cpu.get_hl());
	return dt.str();
}

void update_tiledata()
{
	word_t tile_l;
	word_t tile_h;
	word_t tile_data0, tile_data1;
	for(int tm = 0; tm < 2; ++tm)
	{
		for(int t = 0; t < 256 + 128; ++t)
		{
			size_t tile_off = 8 * (t % 16) + (16 * 8 * 8) * (t / 16); 
			for(int y = 0; y < 8; ++y)
			{
				tile_l = mmu.read_vram(tm, 0x8000 + t * 16 + y * 2);
				tile_h = mmu.read_vram(tm, 0x8000 + t * 16 + y * 2 + 1);
				GPU::palette_translation(tile_l, tile_h, tile_data0, tile_data1);
				for(int x = 0; x < 8; ++x)
				{
					word_t shift = ((7 - x) % 4) * 2;
					word_t color = ((x > 3 ? tile_data1 : tile_data0) >> shift) & 0b11;
					tile_data[tm][tile_off + 16 * 8 * y + x] = std::min((4 - color) * 64, 255);
				}
			}
		}
		gameboy_tiledata[tm].update(reinterpret_cast<uint8_t*>(tile_data[tm]));
	}
}

void update_tilemaps()
{
	word_t tile_l;
	word_t tile_h;
	word_t tile_data0, tile_data1;
	for(int tm = 0; tm < 2; ++tm)
	{
		addr_t mapoffs = (tm == 0) ? 0x9800 : 0x9C00;
		bool select = (gpu.get_lcdc() & GPU::BGWindowsTileDataSelect);
		addr_t base_tile_data = select ? 0x8000 : 0x9000;
		for(int t = 0; t < 32 * 32; ++t)
		{
			size_t tile_off = (8 * 8 * 32) * (t / 32) + 8 * (t % 32);
			for(int y = 0; y < 8; ++y)
			{
				int ti = mmu.read_vram(0, mapoffs + t);
				if(!select && (ti & 0x80))
					 ti = -((~ti + 1) & 0xFF);
				tile_l = mmu.read_vram(tm, base_tile_data + ti * 16 + y * 2);
				tile_h = mmu.read_vram(tm, base_tile_data + ti * 16 + y * 2 + 1);
				GPU::palette_translation(tile_l, tile_h, tile_data0, tile_data1);
				for(int x = 0; x < 8; ++x)
				{
					word_t shift = ((7 - x) % 4) * 2;
					word_t color = ((x > 3 ? tile_data1 : tile_data0) >> shift) & 0b11;
					tile_maps[tm][tile_off + (8 * 32) * y +  + x] = std::min((4 - color) * 64, 255);
				}
			}
		}
		gameboy_tilemap[tm].update(reinterpret_cast<uint8_t*>(tile_maps[tm]));
	}
}

void setup_window()
{
	window.create(sf::VideoMode(
		screen_scale * gpu.ScreenWidth + padding, 
		screen_scale * gpu.ScreenHeight + padding), 
		"SenBoy");
	window.setVerticalSyncEnabled(false);
	
	if(!gameboy_screen.create(gpu.ScreenWidth, gpu.ScreenHeight))
		std::cerr << "Error creating the screen texture!" << std::endl;
	gameboy_screen_sprite.setTexture(gameboy_screen);
	gameboy_screen_sprite.setPosition(padding / 2, 
									padding / 2);
	gameboy_screen_sprite.setScale(screen_scale, screen_scale);
	
	if(mmu.cgb_mode())
		gameboy_logo_tex.loadFromFile(config::to_abs("data/gbc_logo.png"));
	else
		gameboy_logo_tex.loadFromFile(config::to_abs("data/gb_logo.png"));
	gameboy_logo_sprite.setTexture(gameboy_logo_tex);
	gameboy_logo_sprite.setPosition(padding / 2, 
									padding / 2 + screen_scale * gpu.ScreenHeight);
	gameboy_logo_sprite.setScale(screen_scale / 4.0, screen_scale / 4.0);
	
	if(!font.loadFromFile(config::to_abs("data/Hack-Regular.ttf")))
		std::cerr << "Error loading the font!" << std::endl;
}

void setup_debug_window()
{
	auto right = [&](const sf::Sprite& s) -> float { 
		return s.getGlobalBounds().left + s.getGlobalBounds().width;
	};
	
	for(int i = 0; i < 2; ++i)
	{
		if(!gameboy_tiledata[i].create(16 * 8, (8 * 3) * 8))
			std::cerr << "Error creating the vram texture!" << std::endl;
		gameboy_tiledata_sprite[i].setTexture(gameboy_tiledata[i]);
		gameboy_tiledata_sprite[i].setScale(0.5 * screen_scale, 0.5 * screen_scale);
		tile_data[i] = new color_t[(16 * 8) * ((8 * 3) * 8)];
		std::memset(tile_data[i], 128, 4 * (16 * 8) * ((8 * 3) * 8));
	}
	gameboy_tiledata_sprite[0].setPosition(25, padding / 2);
	gameboy_tiledata_sprite[1].setPosition(right(gameboy_tiledata_sprite[0]) + padding * 0.25,
											gameboy_tiledata_sprite[0].getPosition().y);
	
	for(int i = 0; i < 2; ++i)
	{
		if(!gameboy_tilemap[i].create(32 * 8, 32 * 8))
			std::cerr << "Error creating the vram texture!" << std::endl;
		gameboy_tilemap_sprite[i].setTexture(gameboy_tilemap[i]);
		gameboy_tilemap_sprite[i].setScale(0.5 * screen_scale, 0.5 * screen_scale);
		tile_maps[i] = new color_t[32 * 8 * 32 * 8];
		std::memset(tile_maps[i], 128, 32 * 8 * 32 * 8);
	}
	gameboy_tilemap_sprite[0].setPosition(right(gameboy_tiledata_sprite[1]) + padding * 0.25, 
									gameboy_tiledata_sprite[0].getPosition().y);
	gameboy_tilemap_sprite[1].setPosition(right(gameboy_tilemap_sprite[0]) + padding * 0.25, 
									gameboy_tiledata_sprite[0].getPosition().y);
	
	debug_text.setFont(font);
	debug_text.setCharacterSize(16);
	debug_text.setPosition(5, 0);
	
	log_text.setFont(font);
	log_text.setCharacterSize(16);
	log_text.setPosition(5, window.getSize().y - padding * 0.5 + 5);
	log_text.setString("Log");

	tiledata_text[0].setFont(font);
	tiledata_text[0].setCharacterSize(16);
	tiledata_text[0].setPosition(gameboy_tiledata_sprite[0].getGlobalBounds().left,
		gameboy_tiledata_sprite[0].getGlobalBounds().top + gameboy_tiledata_sprite[0].getGlobalBounds().height + 5);
	tiledata_text[0].setString("TileData (Bank 0)");
	
	tiledata_text[1].setFont(font);
	tiledata_text[1].setCharacterSize(16);
	tiledata_text[1].setPosition(gameboy_tiledata_sprite[1].getGlobalBounds().left,
		gameboy_tiledata_sprite[1].getGlobalBounds().top + gameboy_tiledata_sprite[0].getGlobalBounds().height + 5);
	tiledata_text[1].setString("TileData (Bank 1)");
}

