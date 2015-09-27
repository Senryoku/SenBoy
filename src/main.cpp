#include <sstream>
#include <iomanip>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "MMU.hpp"
#include "Cartridge.hpp"
#include "GPU.hpp"
#include "LR35902.hpp"

template<typename T>
struct HexaGen
{
	T		v;
	HexaGen(T _t) : v(_t) {}
};

template<typename T>
std::ostream& operator<<(std::ostream& os, const HexaGen<T>& t)
{
	return os << "0x" << std::hex << std::setw(sizeof(T) * 2) << std::setfill('0') << (int) t.v;
}

using Hexa = HexaGen<LR35902::addr_t>;
using Hexa8 = HexaGen<LR35902::word_t>;

int main(int argc, char* argv[])
{
	std::string path("tests/cpu_instrs/cpu_instrs.gb");
	if(argc > 1)
		path = argv[1];

	Cartridge cartridge;
	if(!cartridge.load(path))
		return 0;

	// Movie Playback (.vbm)
	// Not in sync. Doesn't work.
	bool use_movie = false;
	std::ifstream movie;
	unsigned int movie_start = 0;
	char playback[2];
	if(argc > 2)
	{
		movie.open(argv[2], std::ios::binary);
		if(!movie)
			std::cerr << " Error: Unable to open " << argv[2] << std::endl;
		else {
			char tmp[4];
			movie.seekg(0x03C); // Start of the inputs, 4bytes unisgned int little endian
			movie.read(tmp, 4);
			movie_start = tmp[0] | (tmp[1] << 8) | (tmp[2] << 16) | (tmp[3] << 24);
			movie.seekg(movie_start);
			use_movie = true;
		}
	}
	
	LR35902 cpu;
	MMU mmu;
	GPU gpu;
	
	mmu.cartridge = &cartridge;

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
		mmu.callback_joy_up = [&] () -> bool { return playback[0] & 0x0040; };
		mmu.callback_joy_down = [&] () -> bool { return playback[0] & 0x0080; };
		mmu.callback_joy_right = [&] () -> bool { return playback[0] & 0x0010; };
		mmu.callback_joy_left = [&] () -> bool { return playback[0] & 0x0020; };
		mmu.callback_joy_a = [&] () -> bool { return playback[0] & 0x0001; };
		mmu.callback_joy_b = [&] () -> bool { return playback[0] & 0x0002; };
		mmu.callback_joy_select = [&] () -> bool { return playback[0] & 0x0004; };
		mmu.callback_joy_start = [&] () -> bool { return playback[0] & 0x0008; };
	}

	cpu.mmu = &mmu;
	gpu.mmu = &mmu;
	
	gpu.reset();
	cpu.reset();
	cpu.reset_cart();
	
	cpu.loadBIOS("data/bios.bin");
	
	float screen_scale = 2.0f;
	
	size_t padding = 200;
	sf::RenderWindow window(sf::VideoMode(screen_scale * gpu.ScreenWidth + 1.25 * padding + screen_scale * 16 * 8, screen_scale * gpu.ScreenHeight + padding), "SenBoy");
	window.setVerticalSyncEnabled(false);
	
	sf::Texture	gameboy_screen;
	if(!gameboy_screen.create(gpu.ScreenWidth, gpu.ScreenHeight))
		std::cerr << "Error creating the screen texture!" << std::endl;
	sf::Sprite gameboy_screen_sprite;
	gameboy_screen_sprite.setTexture(gameboy_screen);
	gameboy_screen_sprite.setPosition(padding / 2, 
									padding / 2);
	gameboy_screen_sprite.setScale(screen_scale, screen_scale);
	
	sf::Texture	gameboy_tilemap;
	if(!gameboy_tilemap.create(16 * 8, (8 * 3) * 8))
		std::cerr << "Error creating the vram texture!" << std::endl;
	sf::Sprite gameboy_tilemap_sprite;
	gameboy_tilemap_sprite.setTexture(gameboy_tilemap);
	gameboy_tilemap_sprite.setPosition(padding + screen_scale * gpu.ScreenWidth, 
									0.25 * padding);
	gameboy_tilemap_sprite.setScale(screen_scale, screen_scale);
	GPU::color_t* tile_map = new GPU::color_t[(16 * 8) * ((8 * 3) * 8)];
	std::memset(tile_map, 128, 4 * (16 * 8) * ((8 * 3) * 8));
	
	sf::Font font;
	if(!font.loadFromFile("data/Hack-Regular.ttf"))
		std::cerr << "Error loading the font!" << std::endl;
	
	sf::Text debug_text;
	debug_text.setFont(font);
	debug_text.setCharacterSize(16);
	debug_text.setPosition(5, 0);
	
	sf::Text log_text;
	log_text.setFont(font);
	log_text.setCharacterSize(16);
	log_text.setPosition(5, window.getSize().y - padding * 0.5 + 5);
	log_text.setString("Log");

	sf::Text tilemap_text;
	tilemap_text.setFont(font);
	tilemap_text.setCharacterSize(16);
	tilemap_text.setPosition(gameboy_tilemap_sprite.getGlobalBounds().left,
		gameboy_tilemap_sprite.getGlobalBounds().top - 32);
	tilemap_text.setString("Tiles");
	
	bool debug = true;
	bool step = true;
	bool real_speed = true;
	bool frame_by_frame = true;
	
	size_t frame_skip = 0;
	
	sf::Clock clock;
	double frame_time = 0;
	size_t speed_update = 60;
	double speed = 100;
	uint64_t elapsed_cycles = 0;
	uint64_t elapsed_cycles_frame = 0;
	while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
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
						clock.restart();
						log_text.setString(debug ? "Debugging" : "Running");
						break;
					}
					case sf::Keyboard::BackSpace:
						mmu.reset();
						gpu.reset(); 
						cpu.reset();
						cpu.reset_cart();
						elapsed_cycles = 0;
						clock.restart();
						debug_text.setString("Reset");
						if(use_movie) movie.seekg(movie_start);
						break;
					case sf::Keyboard::Add:
					{
						LR35902::addr_t addr;
						std::cout << "Adding breakpoint. Enter an address: " << std::endl;
						std::cin >> std::hex >> addr;
						cpu.add_breakpoint(addr);
						std::stringstream ss;
						ss << "Added a breakpoint at " << Hexa(addr) << ".";
						log_text.setString(ss.str());
						break;
					}
					case sf::Keyboard::Subtract:
					{
						cpu.clear_breakpoints();
						log_text.setString("Cleared breakpoints.");
						break;
					}
					case sf::Keyboard::M:
					{
						real_speed = !real_speed;
						elapsed_cycles = 0;
						clock.restart();
						log_text.setString(real_speed ? "Running at real speed" : "Running as fast as possible");
						break;
					}
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
		
		if(--speed_update == 0)
		{
			speed_update = 60;
			double t = clock.getElapsedTime().asSeconds();
			speed = 100.0 * (double(elapsed_cycles_frame) / cpu.ClockRate) / (t - frame_time);
			frame_time = t;
			elapsed_cycles_frame = 0;
		}
		
		double gameboy_time = double(elapsed_cycles) / cpu.ClockRate;
		double diff = gameboy_time - clock.getElapsedTime().asSeconds();
		if(real_speed && !debug && diff > 0)
			sf::sleep(sf::seconds(diff));
		if(!debug || step)
		{
			for(size_t i = 0; i < frame_skip + 1; ++i)
			{
				// Get next input from the movie file.
				if(use_movie) movie.read(playback, 2);

				do
				{
					cpu.execute();
					if(cpu.getInstrCycles() == 0)
						break;

					gpu.step(cpu.getInstrCycles(), i == frame_skip);
					elapsed_cycles += cpu.getInstrCycles();
					elapsed_cycles_frame += cpu.getInstrCycles();
			
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
			}
			
			gameboy_screen.update(reinterpret_cast<uint8_t*>(gpu.screen));
			
			frame_by_frame = false;
			step = false;
		}
			
		// Debug display Tilemap
		//if(debug)
		{
			GPU::word_t tile_l;
			GPU::word_t tile_h;
			GPU::word_t tile_data0, tile_data1;
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
						GPU::word_t shift = ((7 - x) % 4) * 2;
						GPU::word_t color = ((x > 3 ? tile_data1 : tile_data0) >> shift) & 0b11;
						tile_map[tile_off + 16 * 8 * y + x] = (4 - color) * (255/4.0);
					}
				}
			}
			gameboy_tilemap.update(reinterpret_cast<uint8_t*>(tile_map));
		}
		
		if(debug)
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
			dt << std::endl;
			dt << std::dec << std::fixed << std::setw(4) << std::setprecision(1) << 100 * gameboy_time / clock.getElapsedTime().asSeconds() << "% GBTime: " << gameboy_time << " RealTime: " << clock.getElapsedTime().asSeconds();
			debug_text.setString(dt.str());
		} else {
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
