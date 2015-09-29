#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include "MMU.hpp"
#include "Cartridge.hpp"
#include "GPU.hpp"
#include "LR35902.hpp"
#include "GBAudioStream.hpp"

// Options
bool use_bios = true;
bool debug = false;			// Pause execution
bool step = false;			// Step to next instruction (in debug)
bool frame_by_frame = true;
bool real_speed = true;
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
sf::RenderWindow window;
sf::Texture	gameboy_screen;
sf::Sprite gameboy_screen_sprite;
sf::Texture	gameboy_tilemap;
sf::Sprite gameboy_tilemap_sprite;
sf::Font font;
sf::Text debug_text;
sf::Text log_text;
sf::Text tilemap_text;

// Timing
sf::Clock timing_clock;
double frame_time = 0;
size_t speed_update = 60;
double speed = 100;
uint64_t elapsed_cycles = 0;
uint64_t speed_mesure_cycles = 0;
size_t frame_count = 0;
	
// Movie Playback (.vbm)
// Not in sync. Doesn't work.
bool use_movie = false;
std::ifstream movie;
unsigned int movie_start = 0;
char playback[2];
	
void handle_event(sf::Event event);
std::string get_debug_text();
void toggle_speed();
void reset();
void get_input_from_movie();
	
int main(int argc, char* argv[])
{
	std::string path("tests/cpu_instrs/cpu_instrs.gb");
	if(argc > 1)
		path = argv[1];
	
	// Linking them all together
	mmu.cartridge = &cartridge;
	cpu.apu = &apu;
	cpu.mmu = &mmu;
	gpu.mmu = &mmu;
	
	// Loading ROM
	if(!cartridge.load(path))
		return 0;
	
	// Audio buffers
	gb_snd_buffer.clock_rate(LR35902::ClockRate);
	gb_snd_buffer.set_sample_rate(44100);
	apu.output(gb_snd_buffer.center(), gb_snd_buffer.left(), gb_snd_buffer.right());

	// Movie loading
	if(argc > 2)
	{
		// VBM
		 movie.open(argv[2], std::ios::binary);
		// BK2
		//movie.open(argv[2]);
		if(!movie) {
			std::cerr << "Error: Unable to open " << argv[2] << std::endl;
		} else {
			// VBM
			char tmp[4];
			movie.seekg(0x03C); // Start of the inputs, 4bytes unsigned int little endian
			movie.read(tmp, 4);
			movie_start = tmp[0] | (tmp[1] << 8) | (tmp[2] << 16) | (tmp[3] << 24);
			movie.seekg(movie_start);
			use_movie = true;
			use_bios = false;
			
			// BK2 (Not the archive, just the Input Log)
			/*movie.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			movie_start = movie.tellg();
			use_movie = true;
			use_bios = false;*/
			
			std::cout << "Loaded movie file '" << argv[2] << "'. Starting in playback mode..." << std::endl;
		}
	}
	
	// Input callbacks
	if(!use_movie)
	{
		mmu.callback_joy_up = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::Y) < -50) return true; return false; };
		mmu.callback_joy_down = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::Y) > 50) return true; return false; };
		mmu.callback_joy_right = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::X) > 50) return true; return false; };
		mmu.callback_joy_left = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::X) < -50) return true; return false; };
		mmu.callback_joy_a = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 0)) return true; return false; };
		mmu.callback_joy_b = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 1)) return true; return false; };
		mmu.callback_joy_select = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 6)) return true; return false; };
		mmu.callback_joy_start = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 7)) return true; return false; };
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
	
	float screen_scale = 2.0f;
	
	size_t padding = 200;
	window.create(sf::VideoMode(
		screen_scale * gpu.ScreenWidth + 1.25 * padding + screen_scale * 16 * 8, 
		screen_scale * gpu.ScreenHeight + padding), 
		"SenBoy");
	window.setVerticalSyncEnabled(false);
	
	if(!gameboy_screen.create(gpu.ScreenWidth, gpu.ScreenHeight))
		std::cerr << "Error creating the screen texture!" << std::endl;
	gameboy_screen_sprite.setTexture(gameboy_screen);
	gameboy_screen_sprite.setPosition(padding / 2, 
									padding / 2);
	gameboy_screen_sprite.setScale(screen_scale, screen_scale);
	
	if(!gameboy_tilemap.create(16 * 8, (8 * 3) * 8))
		std::cerr << "Error creating the vram texture!" << std::endl;
	gameboy_tilemap_sprite.setTexture(gameboy_tilemap);
	gameboy_tilemap_sprite.setPosition(padding + screen_scale * gpu.ScreenWidth, 
									0.25 * padding);
	gameboy_tilemap_sprite.setScale(screen_scale, screen_scale);
	GPU::color_t* tile_map = new GPU::color_t[(16 * 8) * ((8 * 3) * 8)];
	std::memset(tile_map, 128, 4 * (16 * 8) * ((8 * 3) * 8));
	
	if(!font.loadFromFile("data/Hack-Regular.ttf"))
		std::cerr << "Error loading the font!" << std::endl;
	
	debug_text.setFont(font);
	debug_text.setCharacterSize(16);
	debug_text.setPosition(5, 0);
	
	log_text.setFont(font);
	log_text.setCharacterSize(16);
	log_text.setPosition(5, window.getSize().y - padding * 0.5 + 5);
	log_text.setString("Log");

	tilemap_text.setFont(font);
	tilemap_text.setCharacterSize(16);
	tilemap_text.setPosition(gameboy_tilemap_sprite.getGlobalBounds().left,
		gameboy_tilemap_sprite.getGlobalBounds().top - 32);
	tilemap_text.setString("Tiles");
	
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

				do
				{
					cpu.execute();
					if(cpu.getInstrCycles() == 0)
						break;

					gpu.step(cpu.getInstrCycles(), i == frame_skip);
					elapsed_cycles += cpu.getInstrCycles();
					speed_mesure_cycles += cpu.getInstrCycles();
			
					if(cpu.reached_breakpoint())
					{
						std::stringstream ss;
						ss << "Stepped on a breakpoint at " << Hexa(cpu.getPC());
						log_text.setString(ss.str());
						debug = true;
						step = false;
						break;
					}
				} while((!debug || frame_by_frame) && !gpu.completed_frame());
				frame_count++;
			}
				
			bool stereo = apu.end_frame(cpu.frame_cycles);
			gb_snd_buffer.end_frame(cpu.frame_cycles, stereo);
			auto samples_count = gb_snd_buffer.samples_avail();
			if(samples_count > 0)
			{
				samples_count = gb_snd_buffer.read_samples(snd_buffer.add_samples(samples_count), samples_count);
				if(!(snd_buffer.getStatus() == sf::SoundSource::Status::Playing)) snd_buffer.play();
			}
			
			gameboy_screen.update(reinterpret_cast<uint8_t*>(gpu.screen));
			
			frame_by_frame = false;
			step = false;
		}
			
		// Debug display Tilemap
		//if(debug)
		{
			word_t tile_l;
			word_t tile_h;
			word_t tile_data0, tile_data1;
			for(int t = 0; t < 256 + 128; ++t)
			{
				size_t tile_off = 8 * (t % 16) + (16 * 8 * 8) * (t / 16); 
				for(int y = 0; y < 8; ++y)
				{
					tile_l = mmu.read(0x8000 + t * 16 + y * 2);
					tile_h = mmu.read(0x8000 + t * 16 + y * 2 + 1);
					GPU::palette_translation(tile_l, tile_h, tile_data0, tile_data1);
					for(int x = 0; x < 8; ++x)
					{
						word_t shift = ((7 - x) % 4) * 2;
						word_t color = ((x > 3 ? tile_data1 : tile_data0) >> shift) & 0b11;
						tile_map[tile_off + 16 * 8 * y + x] = (4 - color) * (255/4.0);
					}
				}
			}
			gameboy_tilemap.update(reinterpret_cast<uint8_t*>(tile_map));
		}
		
		if(debug)
		{
			debug_text.setString(get_debug_text());
		} else {
			if(--speed_update == 0)
			{
				speed_update = 60;
				double t = timing_clock.getElapsedTime().asSeconds();
				speed = 100.0 * (double(speed_mesure_cycles) / cpu.ClockRate) / (t - frame_time);
				frame_time = t;
				speed_mesure_cycles = 0;
			}
			std::stringstream dt;
			dt << "Speed " << std::dec << std::fixed << std::setw(4) << std::setprecision(1) << speed << "%";
			debug_text.setString(dt.str());
		}
		
        window.clear(sf::Color::Black);
		window.draw(gameboy_screen_sprite);
		window.draw(gameboy_tilemap_sprite);
		window.draw(debug_text);
		window.draw(log_text);
		window.draw(tilemap_text);
        window.display();
    }
	
	delete[] tile_map;
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
			case sf::Keyboard::Return: 
			{
				debug = !debug;
				elapsed_cycles = 0;
				timing_clock.restart();
				log_text.setString(debug ? "Debugging" : "Running");
				break;
			}
			case sf::Keyboard::BackSpace:
				reset();
				break;
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
			case sf::Keyboard::B:
			{
				addr_t addr;
				std::cout << "Adding breakpoint. Enter an address: " << std::endl;
				std::cin >> std::hex >> addr;
				cpu.add_breakpoint(addr);
				std::stringstream ss;
				ss << "Added a breakpoint at " << Hexa(addr) << ".";
				log_text.setString(ss.str());
				break;
			}
			case sf::Keyboard::N:
			{
				cpu.clear_breakpoints();
				log_text.setString("Cleared breakpoints.");
				break;
			}
			case sf::Keyboard::M: toggle_speed(); break;
			case sf::Keyboard::L:
			{
				frame_by_frame = true;
				step = true;
				debug = true;
				log_text.setString("Advancing one frame");
				break;
			}
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
	cpu.reset_cart();
	if(use_bios)
	{
		if(cartridge.getCGBFlag() == Cartridge::Only)
			cpu.loadBIOS("data/gbc_bios.bin");
		else
			cpu.loadBIOS("data/bios.bin");
	}
	elapsed_cycles = 0;
	timing_clock.restart();
	debug_text.setString("Reset");
	if(use_movie) movie.seekg(movie_start);
}

// Get next input from the movie file.
void get_input_from_movie()
{
	// VBM
	if(use_movie) movie.read(playback, 2);
	// BK2
	/*if(use_movie)
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
	}*/
}

std::string get_debug_text()
{
	std::stringstream dt;
	dt << "PC: " << Hexa(cpu.getPC());
	dt << " SP: " << Hexa(cpu.getSP());
	dt << " | OP: " << Hexa8(cpu.getNextOpcode()) << " ";
	if(LR35902::instr_length[cpu.getNextOpcode()] > 1)
		dt << Hexa8(cpu.getNextOperand0()) << " ";
	if(LR35902::instr_length[cpu.getNextOpcode()] > 2)
		dt << Hexa8(cpu.getNextOperand1());
	dt << std::endl;
	dt << "AF: " << Hexa(cpu.getAF());
	dt << " BC: " << Hexa(cpu.getBC());
	dt << " DE: " << Hexa(cpu.getDE());
	dt << " HL: " << Hexa(cpu.getHL());
	if(cpu.check(LR35902::Flag::Zero)) dt << " Z";
	if(cpu.check(LR35902::Flag::Negative)) dt << " N";
	if(cpu.check(LR35902::Flag::HalfCarry)) dt << " HC";
	if(cpu.check(LR35902::Flag::Carry)) dt << " C";
	dt << std::endl;
	dt << "LY: " << Hexa8(gpu.getLine());
	dt << " LCDC: " << Hexa8(gpu.getLCDControl());
	dt << " STAT: " << Hexa8(gpu.getLCDStatus());
	dt << " P1: " << Hexa8(mmu.read(MMU::P1));
	return dt.str();
}
