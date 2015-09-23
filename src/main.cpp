#include <sstream>
#include <iomanip>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "MMU.hpp"
#include "Cartridge.hpp"
#include "GPU.hpp"
#include "Z80.hpp"

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

using Hexa = HexaGen<Z80::addr_t>;
using Hexa8 = HexaGen<Z80::word_t>;

int main(int argc, char* argv[])
{
	std::string path("tests/cpu_instrs/cpu_instrs.gb");
	if(argc > 1)
		path = argv[1];
	
	Z80 cpu;
	Cartridge cartridge(path);
	MMU mmu;
	GPU gpu;
	
	mmu.cartridge = &cartridge;
	cpu.mmu = &mmu;
	gpu.mmu = &mmu;
	
	gpu.reset();
	cpu.reset();
	cpu.reset_cart();
	
	cpu.loadBIOS("data/bios.bin");
	
	float screen_scale = 2.0f;
	
	size_t padding = 400;
	sf::RenderWindow window(sf::VideoMode(gpu.ScreenWidth + padding + 600, gpu.ScreenHeight + padding), "SenBoy");
	window.setVerticalSyncEnabled(false);
	
	sf::Texture	gameboy_screen;
	if(!gameboy_screen.create(gpu.ScreenWidth, gpu.ScreenHeight))
		std::cerr << "Error creating the screen texture!" << std::endl;
	sf::Sprite gameboy_screen_sprite;
	gameboy_screen_sprite.setTexture(gameboy_screen);
	gameboy_screen_sprite.setPosition(padding / 2 - (screen_scale - 1.0) * gpu.ScreenWidth * 0.5, 
									padding / 2 - (screen_scale - 1.0) * gpu.ScreenHeight * 0.5);
	gameboy_screen_sprite.setScale(screen_scale, screen_scale);
	
	sf::Texture	gameboy_tilemap;
	if(!gameboy_tilemap.create(16 * 8, (8 * 3) * 8))
		std::cerr << "Error creating the vram texture!" << std::endl;
	sf::Sprite gameboy_tilemap_sprite;
	gameboy_tilemap_sprite.setTexture(gameboy_tilemap);
	gameboy_tilemap_sprite.setPosition(600, 
									padding / 2 - (screen_scale - 1.0) * gpu.ScreenHeight * 0.5);
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
	log_text.setPosition(5, 450);
	log_text.setString("Log");
	
	bool first_loop = true;
	bool debug = true;
	bool draw_tilemap = true;
	bool draw_debug_text = true;
	bool step = true;
	
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
						log_text.setString(debug ? "Debugging" : "Running");
						break;
					}
					case sf::Keyboard::BackSpace:
						mmu.reset();
						gpu.reset(); 
						cpu.reset();
						cpu.reset_cart();
						debug_text.setString("Reset");
						break;
					case sf::Keyboard::Add:
					{
						Z80::addr_t addr;
						std::cout << "Adding breakpoint. Enter an address: " << std::endl;
						std::cin >> std::hex >> addr;
						cpu.addBreakpoint(addr);
						std::stringstream ss;
						ss << "Added a breakpoint at " << Hexa(addr) << ".";
						log_text.setString(ss.str());
						break;
					}
					case sf::Keyboard::Subtract:
					{
						cpu.clearBreakpoints();
						log_text.setString("Cleared breakpoints.");
						break;
					}
					case sf::Keyboard::A:
					{
						mmu.key_down(MMU::Button, MMU::RightA);
						break;
					}
					case sf::Keyboard::T:
					{
						draw_tilemap = !draw_tilemap;
						log_text.setString(draw_tilemap ? "Debug Text Enabled" : "Debug Text Disabled");
						break;
					}
					case sf::Keyboard::Y:
					{
						draw_debug_text = !draw_debug_text;
						log_text.setString(draw_debug_text ? "Debug Tilemap Enabled" : "Debug Tilemap Disabled");
						break;
					}
					default: break;
				}
			} else if (event.type == sf::Event::KeyReleased) {
				switch(event.key.code)
				{
					case sf::Keyboard::A:
					{
						mmu.key_up(MMU::Button, MMU::RightA);
						break;
					}
					default: break;
				}
			} else if (event.type == sf::Event::JoystickButtonPressed) {
				switch(event.joystickButton.button)
				{
					case 0: mmu.key_down(MMU::Button, MMU::RightA); break;
					case 2:
					case 1: mmu.key_down(MMU::Button, MMU::LeftB); break;
					case 6: mmu.key_down(MMU::Button, MMU::UpSelect); break;
					case 7: mmu.key_down(MMU::Button, MMU::DownStart); break;
					default: break;
				}
			} else if (event.type == sf::Event::JoystickMoved) {
				if (event.joystickMove.axis == sf::Joystick::X)
				{
					if(event.joystickMove.position > 50) mmu.key_down(MMU::Direction, MMU::RightA);
					else if(event.joystickMove.position < -50) mmu.key_down(MMU::Direction, MMU::LeftB);
					else {
						mmu.key_up(MMU::Direction, MMU::RightA);
						mmu.key_up(MMU::Direction, MMU::LeftB);
					}
				} else if (event.joystickMove.axis == sf::Joystick::Y) {
					if(event.joystickMove.position > 50) mmu.key_down(MMU::Direction, MMU::UpSelect);
					else if(event.joystickMove.position < -50) mmu.key_down(MMU::Direction, MMU::DownStart);
					else {
						mmu.key_up(MMU::Direction, MMU::UpSelect);
						mmu.key_up(MMU::Direction, MMU::DownStart);
					}
				}
			}
        }
		
		///////////////////////////
		// (Input Tests)
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::Q))
		{
			mmu.key_down(MMU::Button, MMU::DownStart);
		}
		
		if(sf::Joystick::getAxisPosition(0, sf::Joystick::X) > 50) mmu.key_down(MMU::Direction, MMU::RightA);
		else if(sf::Joystick::getAxisPosition(0, sf::Joystick::X) < -50) mmu.key_down(MMU::Direction, MMU::LeftB);
		else {
			mmu.key_up(MMU::Direction, MMU::RightA);
			mmu.key_up(MMU::Direction, MMU::LeftB);
		}
		///////////////////////////
		
		if(!debug || step)
		{
			do
			{
				cpu.execute();
				if(cpu.getInstrCycles() == 0)
					break;
				
				gpu.step(cpu.getInstrCycles());
			
				if(cpu.reachedBreakpoint())
				{
					std::stringstream ss;
					ss << "Stepped on a breakpoint at " << Hexa(cpu.getPC());
					log_text.setString(ss.str());
					debug = true;
					step = false;
					break;
				}
			} while(!debug && !gpu.completed_frame());
			
			gameboy_screen.update(reinterpret_cast<uint8_t*>(gpu.screen));
			
			step = false;
		}
			
		// Debug display Tilemap
		if(draw_tilemap || first_loop)
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
		
		if(draw_debug_text || first_loop)
		{
			std::stringstream dt;
			dt << "PC: " << Hexa(cpu.getPC());
			dt << " SP: " << Hexa(cpu.getSP());
			dt << " | OP: " << Hexa8(cpu.getNextOpcode()) << " ";
			if(Z80::instr_length[cpu.getNextOpcode()] > 1)
				dt << Hexa8(cpu.getNextOperand0()) << " ";
			if(Z80::instr_length[cpu.getNextOpcode()] > 2)
				dt << Hexa8(cpu.getNextOperand1());
			dt << std::endl;
			dt << "AF: " << Hexa(cpu.getAF());
			dt << " BC: " << Hexa(cpu.getBC());
			dt << " DE: " << Hexa(cpu.getDE());
			dt << " HL: " << Hexa(cpu.getHL());
			if(cpu.check(Z80::Flag::Zero)) dt << " Z";
			if(cpu.check(Z80::Flag::Negative)) dt << " N";
			if(cpu.check(Z80::Flag::HalfCarry)) dt << " HC";
			if(cpu.check(Z80::Flag::Carry)) dt << " C";
			dt << std::endl;
			dt << "LY: " << Hexa8(gpu.getLine());
			dt << " LCDC: " << Hexa8(gpu.getLCDControl());
			dt << " STAT: " << Hexa8(gpu.getLCDStatus());
			dt << " P1: " << Hexa8(mmu.read(MMU::P1));
			debug_text.setString(dt.str());
		}
		
        window.clear(sf::Color::Black);
		window.draw(gameboy_screen_sprite);
		window.draw(gameboy_tilemap_sprite);
		window.draw(debug_text);
		window.draw(log_text);
        window.display();
		
		first_loop = false;
    }
	
	delete[] tile_map;
}