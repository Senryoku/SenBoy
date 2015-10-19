#include "SenBoy.hpp"

///////////////////////////////////////////////////////////////////////////////
//

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
bool emulation_started = false;
Cartridge	cartridge;
LR35902		cpu;
Gb_Apu		apu;
MMU			mmu;
GPU			gpu;
	
Stereo_Buffer gb_snd_buffer;
GBAudioStream snd_buffer;


// GUI
float screen_scale = 3.0f;

// Debug GUI
/*
sf::Texture	gameboy_tilemap;
sf::Sprite gameboy_tilemap_sprite;
sf::Texture	gameboy_tilemap2;
sf::Sprite gameboy_tilemap_sprite2;
color_t* tile_map[2] = {nullptr, nullptr};
*/
// Timing
sf::Clock timing_clock;
double frame_time = 0;
size_t speed_update = 10;
double speed = 100;
uint64_t elapsed_cycles = 0;
uint64_t speed_mesure_cycles = 0;
size_t frame_count = 0;
	
void handle_event(sf::Event event);
std::string get_debug_text();
void toggle_speed();
void reset();
void get_input_from_movie();

///////////////////////////////////////////////////////////////////////////////
// Canvas (SFML)

MainCanvas::MainCanvas(wxWindow*	Parent,
						 wxWindowID	Id,
						 wxPoint	Position,
						 wxSize		Size,
						 long		Style) :
	wxSFMLCanvas(Parent, Id, Position, Size, Style)
{
	_parent = static_cast<wxFrame*>(Parent);
}

void MainCanvas::init()
{
	setVerticalSyncEnabled(false);
	
	if(!gameboy_screen.create(gpu.ScreenWidth, gpu.ScreenHeight))
	{
		std::cerr << "Error creating the screen texture!" << std::endl;
		close();
	}
	gameboy_screen_sprite.setTexture(gameboy_screen);
	gameboy_screen_sprite.setPosition(0, 0);
	gameboy_screen_sprite.setScale(screen_scale, screen_scale);
	
	mmu.cartridge = &cartridge;
	if(with_sound) cpu.apu = &apu;
	cpu.mmu = &mmu;
	gpu.mmu = &mmu;
	
	// Audio buffers
	gb_snd_buffer.clock_rate(LR35902::ClockRate);
	gb_snd_buffer.set_sample_rate(sample_rate);
	apu.output(gb_snd_buffer.center(), gb_snd_buffer.left(), gb_snd_buffer.right());
	snd_buffer.setVolume(50);
	
	mmu.callback_joy_up = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::Y) < -50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Up); };
	mmu.callback_joy_down = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::Y) > 50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Down); };
	mmu.callback_joy_right = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::X) > 50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Right); };
	mmu.callback_joy_left = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::X) < -50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Left); };
	mmu.callback_joy_a = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 0)) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::A); };
	mmu.callback_joy_b = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 1)) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Z); };
	mmu.callback_joy_select = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 6)) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Q); };
	mmu.callback_joy_start = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 7)) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::S); };
}
	
void MainCanvas::OnUpdate()
{
	if(!emulation_started) return; 
	
	while(pollEvent(_event))
		handle_event(_event);
	
	double gameboy_time = double(elapsed_cycles) / cpu.ClockRate;
	double diff = gameboy_time - timing_clock.getElapsedTime().asSeconds();
	if(real_speed && !debug && diff > 0)
		sf::sleep(sf::seconds(diff));
	if(!debug || step)
	{
		cpu.frame_cycles = 0;
		for(size_t i = 0; i < frame_skip + 1; ++i)
		{
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
					_parent->SetStatusText(ss.str());
					debug = true;
					step = false;
					break;
				}
			} while((!debug || frame_by_frame) && (!gpu.completed_frame() || elapsed_cycles < 70224));
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
	
	if(--speed_update == 0)
	{
		speed_update = 10;
		double t = timing_clock.getElapsedTime().asSeconds();
		speed = 100.0 * (double(speed_mesure_cycles) / cpu.ClockRate) / (t - frame_time);
		frame_time = t;
		speed_mesure_cycles = 0;
		std::stringstream dt;
		dt << "Speed " << std::dec << std::fixed << std::setw(4) << std::setprecision(1) << speed << "%";
		setTitle(std::string("SenBoy - ").append(dt.str()));
	}

	setActive(true);
	clear(sf::Color::Black);
	draw(gameboy_screen_sprite); // Crash here when trying to bind the texture...
}

void MainCanvas::handle_event(sf::Event event)
{
	if(event.type == sf::Event::Closed)
		close();
	if(event.type == sf::Event::KeyPressed)
	{
		switch(event.key.code)
		{
			case sf::Keyboard::Escape: close(); break;
			case sf::Keyboard::Space: step = true; break;
			case sf::Keyboard::Return: 
			{
				debug = !debug;
				elapsed_cycles = 0;
				timing_clock.restart();
				_parent->SetStatusText(debug ? "Debugging" : "Running");
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
				_parent->SetStatusText(std::string("Volume at ").append(std::to_string(vol)));
				break;
			}
			case sf::Keyboard::Subtract:
			{
				float vol = snd_buffer.getVolume();
				vol = std::max(vol - 10.0f, 0.0f);
				snd_buffer.setVolume(vol);
				_parent->SetStatusText(std::string("Volume at ").append(std::to_string(vol)));
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
				_parent->SetStatusText(ss.str());
				break;
			}
			case sf::Keyboard::N:
			{
				cpu.clear_breakpoints();
				_parent->SetStatusText("Cleared breakpoints.");
				break;
			}
			case sf::Keyboard::M: toggle_speed(); break;
			case sf::Keyboard::L:
			{
				frame_by_frame = true;
				step = true;
				debug = true;
				_parent->SetStatusText("Advancing one frame");
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
}

void reset()
{
	gpu.reset();
	cpu.reset();
	cpu.reset_cart();
	if(use_bios)
	{
		if(mmu.cgb_mode())
			cpu.loadBIOS(config::to_abs("data/gbc_bios.bin"));
		else
			cpu.loadBIOS(config::to_abs("data/bios.bin"));
	}
	elapsed_cycles = 0;
	timing_clock.restart();
	size_t frame_cycles = (cpu.double_speed() ? cpu.frame_cycles / 2 : cpu.frame_cycles);
	apu.end_frame(frame_cycles);
}

///////////////////////////////////////////////////////////////////////////////
// Frame

enum Actions
{
	ID_OPEN
};

MainFrame::MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
        : wxFrame(NULL, wxID_ANY, title, pos, size)
{	
	auto _canvas = new MainCanvas(this, wxID_ANY, wxPoint(50, 50), size);
	_canvas->init();

	wxMenu *menuFile = new wxMenu;
    menuFile->Append(ID_OPEN, "&Open...\tCtrl-H",
                     "Open ROM and start emulation.");
	menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);
    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
	
    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");
    SetMenuBar(menuBar);
	
    CreateStatusBar();
    SetStatusText("Oh, hi!");
}

void MainFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox("SenBoy v0.91\nA basic GameBoy emulator by Senryoku.\nFor more informations and lastest version: https://github.com/Senryoku/SenBoy",
                 "About SenBoy", wxOK | wxICON_INFORMATION );
}

void MainFrame::OnOpen(wxCommandEvent&)
{
	cartridge.save();
	wxFileDialog openFileDialog(this, _("Open ROM file"), "", "",
				   "GameBoy (Color) files (*.gb)|*.gb", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (openFileDialog.ShowModal() == wxID_CANCEL)
        return;     // the user changed idea...

    if(!cartridge.load(static_cast<const char*>(openFileDialog.GetPath().c_str())))
    {
        wxLogError("Error opening ROM '%s'.", openFileDialog.GetPath());
        return;
    } else {
		SetStatusText("Loaded " + cartridge.getName());
		reset();
		
		emulation_started = true;
	}
}

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(ID_OPEN,  MainFrame::OnOpen)
    EVT_MENU(wxID_EXIT,  MainFrame::OnExit)
    EVT_MENU(wxID_ABOUT, MainFrame::OnAbout)
wxEND_EVENT_TABLE()

///////////////////////////////////////////////////////////////////////////////
// Application

bool SenBoy::OnInit()
{
	config::set_folder(argv[0]);
	
    MainFrame* frame = new MainFrame("SenBoy", wxPoint(50, 50), 
									wxSize(screen_scale * gpu.ScreenWidth, 
										screen_scale * gpu.ScreenHeight));
    frame->Show( true );
    return true;
}

wxIMPLEMENT_APP(SenBoy);
