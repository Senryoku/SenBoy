#include "Z80.hpp"

#include <sstream>
#include <iomanip>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

template<typename T>
struct HexaGen
{
	T		v;
	size_t	s = 4;
	HexaGen(T _t, size_t _s = 4) : v(_t), s(_s) {}
};

template<typename T>
std::ostream& operator<< (std::ostream& os, const HexaGen<T>& t)
{
	return os << "0x" << std::hex << std::setw(t.s) << std::setfill('0') << (int) t.v;
}

using Hexa = HexaGen<Z80::addr_t>;

int main(int argc, char* argv[])
{
	std::string path("tests/cpu_instrs/cpu_instrs.gb");
	if(argc > 1)
		path = argv[1];
	
	Z80 cpu;
	ROM rom(path);
	GPU gpu;
	gpu.reset();
	
	cpu.rom = &rom;
	cpu.gpu = &gpu;
	cpu.reset();
	cpu.reset_cart();
	
	float screen_scale = 2.0f;
	
	size_t padding = 400;
	sf::RenderWindow window(sf::VideoMode(gpu.ScreenWidth + padding, gpu.ScreenHeight + padding), "SenBoy");
	window.setVerticalSyncEnabled(false);
	
	sf::Texture	gameboy_screen;
	if(!gameboy_screen.create(gpu.ScreenWidth, gpu.ScreenHeight))
		std::cerr << "Error creating the screen texture!" << std::endl;
	sf::Sprite gameboy_screen_sprite;
	gameboy_screen_sprite.setTexture(gameboy_screen);
	gameboy_screen_sprite.setPosition(padding / 2 - (screen_scale - 1.0) * gpu.ScreenWidth * 0.5, 
									padding / 2 - (screen_scale - 1.0) * gpu.ScreenHeight * 0.5);
	gameboy_screen_sprite.setScale(screen_scale, screen_scale);
	
	sf::Text debug_text;
	sf::Font font;
	if(!font.loadFromFile("data/Hack-Regular.ttf"))
		std::cerr << "Error loading the font!" << std::endl;
	debug_text.setFont(font);
	debug_text.setCharacterSize(16);
	
	bool debug = true;
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
					case sf::Keyboard::Return: debug = !debug; break;
					case sf::Keyboard::BackSpace: 
						gpu.reset(); 
						cpu.reset_cart();
						debug_text.setString("Reset");
						break;
				}
			}
        }
		
		if(!debug || step)
		{
			cpu.execute();
			for(int i = 0; i < cpu.getInstrCycles(); ++i)
				gpu.step();
			
			gameboy_screen.update(reinterpret_cast<uint8_t*>(gpu.screen));
			
			std::stringstream dt;
			dt << "PC: " << Hexa(cpu.getPC());
			dt << " SP: " << Hexa(cpu.getSP());
			dt << " | OP: " << Hexa(cpu.getNextOpcode()) << " [" 
				<< Hexa(cpu.getNextOperand0()) << " " 
				<< Hexa(cpu.getNextOperand1()) << "]" << std::endl;
			dt << "AF: " << Hexa(cpu.getAF());
			dt << " BC: " << Hexa(cpu.getBC());
			dt << " DE: " << Hexa(cpu.getDE());
			dt << " HL: " << Hexa(cpu.getHL());
			debug_text.setString(dt.str());
			step = false;
		}
		
        window.clear(sf::Color::Black);
		window.draw(gameboy_screen_sprite);
		window.draw(debug_text);
        window.display();
    }
}