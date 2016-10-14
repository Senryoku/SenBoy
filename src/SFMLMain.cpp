#include <experimental/filesystem>
#include <deque>
#include <sstream>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <imgui.h>
#include <imgui-SFML.h>

#include <Core/GameBoy.hpp>
#include <GBAudioStream.hpp>

#include <Tools/CommandLine.hpp>
#include <Tools/Config.hpp>

#include <Analysis/Analyser.hpp>

// Options
bool fullscreen = false;
bool post_process = false;
float blend_speed = 0.70f;
bool show_gui = false;
bool use_boot = true;
bool custom_boot = false;
bool with_sound = true;
bool debug = false;			// Pause execution
bool step = false;			// Step to next instruction (in debug)
bool frame_by_frame = true;
bool real_speed = true;
size_t sample_rate = 44100;	// Audio sample rate
size_t frame_skip = 0;		// Increase for better performances.
std::string rom_path("");
std::deque<std::string> logs;
std::ostringstream log_line;

// Components of the emulated GameBoy
Cartridge	cartridge;
MMU			mmu(cartridge);
Gb_Apu		apu;
LR35902		cpu(mmu, apu);
GPU			gpu(mmu);

Stereo_Buffer gb_snd_buffer;
GBAudioStream snd_buffer;

// GUI
sf::RenderWindow window;
sf::Texture	gameboy_screen;
sf::Sprite gameboy_screen_sprite;
std::unique_ptr<color_t[]> screen_buffer;

// Debug GUI
sf::Texture	gameboy_tiledata[2];
sf::Sprite gameboy_tiledata_sprite[2];
sf::Texture	gameboy_tilemap[2];
sf::Sprite gameboy_tilemap_sprite[2];
std::unique_ptr<color_t[]> tile_data[2];
std::unique_ptr<color_t[]> tile_maps[2];

// Timing
sf::Clock timing_clock;
sf::Clock delta_clock; // GUI Clock
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
MovieType	movie_type = SBM;

Analyser analyser;

void help();
void toggleFullscreen();
void setup_window();
void gui();
void handle_event(sf::Event event);
void handle_event_debug(sf::Event event);
void toggle_speed();
void reset();
void load_empty_rom();
void clear_breakpoints();
void modify_volume(float v);
void load_movie(const char* movie_path);
void get_input_from_movie();
void movie_save_frame();
void update_tiledata();
void update_tilemaps();
void set_input_callbacks();

template<typename T, typename ...Args>
void log(const T& msg, Args... args);

int main(int argc, char* argv[])
{
	// Command Line Options
	config::set_folder(argv[0]);

	if(has_option(argc, argv, "-h")) { help(); return 0; }
	
	char* cli_rom_path = get_file(argc, argv);
	if(cli_rom_path)
	{
		rom_path = cli_rom_path;
	} else {
		log("No ROM specified.");
		show_gui = true;
	}
	
	if(has_option(argc, argv, "-d"))
		debug = true;
	if(has_option(argc, argv, "-b"))
		use_boot = false;
	if(has_option(argc, argv, "-s"))
		with_sound = false;
	if(has_option(argc, argv, "--dmg"))
		mmu.force_dmg = true;
	if(has_option(argc, argv, "--cgb"))
		mmu.force_cgb = true;
	
	// Audio buffers
	gb_snd_buffer.clock_rate(LR35902::ClockRate);
	gb_snd_buffer.set_sample_rate(sample_rate);
	apu.output(gb_snd_buffer.center(), gb_snd_buffer.left(), gb_snd_buffer.right());
	snd_buffer.setVolume(50);

	screen_buffer.reset(new color_t[gpu.ScreenWidth * gpu.ScreenHeight]);
	
	// Movie Saving
	const char* movie_save_path = get_option(argc, argv, "$ms");
	if(movie_save_path)
		movie_save.open(movie_save_path, std::ios::binary);
	
	// Movie loading
	const char* movie_path = get_option(argc, argv, "$m");
	if(movie_path)
		load_movie(movie_path);
	
	///////////////////////////////////////////////////////////////////////////
	// Setting up GUI
	
	setup_window();
    ImGui::SFML::Init(window);

	if(!gameboy_screen.create(gpu.ScreenWidth, gpu.ScreenHeight))
		log("Error creating the screen texture!");
	gameboy_screen_sprite.setTexture(gameboy_screen);
	
	for(int i = 0; i < 2; ++i)
	{
		const size_t s[2] = {16 * 8, (8 * 3) * 8};
		if(!gameboy_tiledata[i].create(s[0], s[1]))
			log("Error creating the vram texture!");
		gameboy_tiledata_sprite[i].setTexture(gameboy_tiledata[i]);
		
		tile_data[i].reset(new color_t[s[0] * s[1]]);
		std::memset(tile_data[i].get(), 128, 4 * s[0] * s[1]);

		const size_t s2 = 32 * 8;
		if(!gameboy_tilemap[i].create(s2, s2))
			log("Error creating the vram texture!");
		gameboy_tilemap_sprite[i].setTexture(gameboy_tilemap[i]);
		
		tile_maps[i].reset(new color_t[s2 * s2]);
		std::memset(tile_maps[i].get(), 128, 4 * s2 * s2);
	}
	
	cartridge.Log = log<const std::string&>;

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
						log("Stepped on a breakpoint at ", Hexa(cpu.get_pc()));
						debug = true;
						step = false;
						break;
					}
				} while((!debug || frame_by_frame) && (!gpu.completed_frame() && cpu.frame_cycles <= 70224));

				frame_count++;
			}
			
			size_t frame_cycles = cpu.frame_cycles;
			bool stereo = apu.end_frame(frame_cycles);
			if(with_sound)
			{
				gb_snd_buffer.end_frame(frame_cycles, stereo);
				size_t samples_count = gb_snd_buffer.samples_avail();
				if(samples_count > 0)
				{
					if(samples_count >= snd_buffer.buffer_size)
					{
						log("Warning: Way too many sound samples at once. Discarding some...");
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
			// Must be done here, or any subsequent call to read could cause the APU to advance by one frame...
			// (and violate some internal requirements of Gb_Apu (end_time > last_time))
			cpu.frame_cycles = 0;
			
			if(post_process)
			{
				for(unsigned int i = 0; i < gpu.ScreenWidth * gpu.ScreenHeight; ++i) // Extremely basic, LCDs don't work like this.
					screen_buffer[i] = (1.0f - blend_speed) * screen_buffer[i] + blend_speed * gpu.get_screen()[i];
				gameboy_screen.update(reinterpret_cast<const uint8_t*>(screen_buffer.get()));
			} else {
				gameboy_screen.update(reinterpret_cast<const uint8_t*>(gpu.get_screen()));
			}
			
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
		
        window.clear(sf::Color::Black);
		auto win_size = window.getView().getSize();
		auto sprite_lb = gameboy_screen_sprite.getLocalBounds();
		float scale = std::min(win_size.x / sprite_lb.width, 
			win_size.y / sprite_lb.height);
		scale = std::max(scale, 1.0f);
		gameboy_screen_sprite.setScale(scale, scale);
		auto sprite_gb = gameboy_screen_sprite.getGlobalBounds();
		gameboy_screen_sprite.setPosition(win_size.x / 2 - sprite_gb.width / 2, 
			win_size.y / 2 - sprite_gb.height / 2);
		if(show_gui) gameboy_screen_sprite.setColor(sf::Color{255, 255, 255, 128});
		else gameboy_screen_sprite.setColor(sf::Color::White);
		window.draw(gameboy_screen_sprite);

		if(show_gui) gui();
		
        window.display();
    }
	
	cartridge.save();
}

void help()
{
	std::cout << "------------------------------------------------------------" << std::endl
			<< " SenBoy - Help" << std::endl
			<< " Basic usage:" << std::endl
			<< "  ./SenBoy \"path/to/rom\" [-options]" << std::endl
			<< " Supported options:" << std::endl
			<< "  -d \t\tStart in debug." << std::endl
			<< "  -b \t\tSkip Boot ROM." << std::endl
			<< "  -s \t\tDisable sound." << std::endl
			//<< "  $m \"path\" \tSpecify a input movie file." << std::endl
			//<< "  $ms \"path\" \tSpecify a output movie file." << std::endl
			<< "  --dmg \tForce DMG mode." << std::endl
			<< "  --cgb \tForce CGB mode." << std::endl
			<< " See the README.txt for more and up-to-date informations." << std::endl
			<< "------------------------------------------------------------" << std::endl;
}

void toggle_debug()
{
	debug = !debug;
	elapsed_cycles = 0;
	timing_clock.restart();
	log(debug ? "Debugging" : "Running");
}

std::experimental::filesystem::path explore(const std::experimental::filesystem::path& path, const std::vector<const char*>& extensions = {})
{
	namespace fs = std::experimental::filesystem;
	fs::path return_path;
	if(!fs::is_directory(path))
		return return_path;
	for(auto& p: fs::directory_iterator(path))
	{
		if(fs::is_directory(p))
		{
			if(ImGui::TreeNode(p.path().filename().string().c_str()))
			{
				auto tmp_r = explore(p.path(), extensions);
				if(!tmp_r.empty())
					return_path = tmp_r;
				ImGui::TreePop();
			}
		} else {
			auto ext = p.path().extension().string();
			if(extensions.empty() || std::find(extensions.begin(), extensions.end(), ext) != extensions.end())
				if(ImGui::SmallButton(p.path().filename().string().c_str()))
					return_path = p.path();
		}
	}
	return return_path;
}

void gui()
{
	ImGui::SFML::Update(delta_clock.restart());
	
	static bool log_window = false;
	static bool gameboy_window = false;
	static bool debug_window = false;
	
	bool open_rom_popup = false;
	bool open_movie_popup = false;
	bool about_popup = false;
	if(ImGui::BeginMainMenuBar())
	{
		if(ImGui::BeginMenu("File"))
		{
			open_rom_popup = ImGui::MenuItem("Open ROM");
			//open_movie_popup = ImGui::MenuItem("Open Movie"); // @todo Debug then re-enable...
			ImGui::Separator();
			if(ImGui::MenuItem("Save", "Ctrl+S"))
				cartridge.save();
			if(ImGui::MenuItem("Reset"))
				reset();
			if(ImGui::MenuItem("Exit", "Ctrl+Q"))
				window.close();
			ImGui::EndMenu();
		}
		
		if(ImGui::BeginMenu("Options"))
		{
			if(ImGui::MenuItem("Fullscreen", "Alt+Enter")) toggleFullscreen();
			if(ImGui::MenuItem("Post-processing", "P")) post_process = !post_process;
			ImGui::Separator();
			if(ImGui::MenuItem("Pause", "D")) debug = !debug;
			if(ImGui::MenuItem("Real Speed", "M")) toggle_speed();
			ImGui::Separator();
			ImGui::Checkbox("Enable Sound", &with_sound);
			float vol = snd_buffer.getVolume();
			if(ImGui::SliderFloat("Volume", &vol, 0.f, 100.f, "%.0f"))
			{
				vol = std::max(0.0f, std::min(vol, 100.0f));
				snd_buffer.setVolume(vol);
			}
			ImGui::Separator();
			ImGui::Checkbox("Use Boot ROM", &use_boot);
			if(ImGui::RadioButton("Original", !custom_boot))
				custom_boot = false;
			if(ImGui::RadioButton("Custom", custom_boot))
				custom_boot = true;
			ImGui::EndMenu();
		}
		
		if(ImGui::BeginMenu("Views"))
		{
			if(ImGui::MenuItem("Logs")) log_window = !log_window;
			if(ImGui::MenuItem("GameBoy")) gameboy_window = !gameboy_window;
			if(ImGui::MenuItem("Debug")) debug_window = !debug_window;
			ImGui::EndMenu();
		}
		
		if(ImGui::BeginMenu("Help"))
		{
			about_popup = ImGui::MenuItem("About##menuitem");
			ImGui::EndMenu();
		}
						
		ImGui::EndMainMenuBar();
	}
	
	if(about_popup) ImGui::OpenPopup("About");
	if(ImGui::BeginPopupModal("About"))
	{
		ImGui::Text("SenBoy - A basic GameBoy emulator by Senryoku.");
		ImGui::Text("https://github.com/Senryoku/SenBoy");
		if(ImGui::Button("Ok"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	
	if(open_rom_popup) ImGui::OpenPopup("Open ROM");
	if(ImGui::BeginPopup("Open ROM"))
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.0, 0.0, 0.0, 0.0});
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.3647, 0.3607, 0.7176, 1.0});
		namespace fs = std::experimental::filesystem;
		static char root_path[256] = ".";
		ImGui::InputText("Root", root_path, 256);
		auto p = explore(root_path, {".gb", ".gbc"});
		if(!p.empty())
		{
			cartridge.save();
			rom_path = p.string();
			reset();
			show_gui = false;
			// @todo Not so sure about that...
			ImGui::GetIO().WantCaptureKeyboard = ImGui::GetIO().WantTextInput = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::PopStyleColor(2);
		if(ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	
	if(open_movie_popup) ImGui::OpenPopup("Open Movie");
	if(ImGui::BeginPopup("Open Movie"))
	{
		namespace fs = std::experimental::filesystem;
		static char root_path[256] = ".";
		ImGui::InputText("Root", root_path, 256);
		auto p = explore(root_path, {".sbm", ".vbm", ".bkm"});
		if(!p.empty())
		{
			load_movie(p.string().c_str());
			reset();
			show_gui = false;
			// @todo Not so sure about that...
			ImGui::GetIO().WantCaptureKeyboard = ImGui::GetIO().WantTextInput = false;
			ImGui::CloseCurrentPopup();
		}
		if(ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	
	if(log_window)
	{
		ImGui::Begin("Logs", &log_window);
		ImGuiListClipper clipper(logs.size());
			while(clipper.Step())
				for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
					ImGui::Text("%s", logs[i].c_str());
		ImGui::End();
	}
	
	if(gameboy_window)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(gpu.ScreenWidth, 
			gpu.ScreenHeight + 19)); // Why 19? You tell me... (Title bar ?)
		ImGui::Begin("GameBoy", &gameboy_window, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
		ImVec2 win_size = ImGui::GetWindowContentRegionMax();
		win_size.x -= ImGui::GetWindowContentRegionMin().x;
		win_size.y -= ImGui::GetWindowContentRegionMin().y;
		float scale = std::min(win_size.x / gpu.ScreenWidth, win_size.y / gpu.ScreenHeight);
		scale = std::max(scale, 1.0f);
		gameboy_screen_sprite.setScale(scale, scale);
		ImGui::Image(gameboy_screen_sprite);
		ImGui::End();
		ImGui::PopStyleVar(1);
	}
	
	if(debug_window)
	{
		ImGui::Begin("Debug informations", &debug_window, ImGuiWindowFlags_NoCollapse);
		ImGui::Value("Debugging", debug);
		if(ImGui::Button("Toggle Debugging (D)"))
			debug = !debug;
		if(debug)
		{
			ImGui::SameLine();
			if(ImGui::Button("Step (Space)"))
				step = true;
		}
		if(ImGui::CollapsingHeader("CPU"))
		{
			ImGui::Text("%s: 0x%04X", "PC", cpu.get_pc());
			ImGui::Text("%s: 0x%04X", "SP", cpu.get_sp());
			ImGui::Text("%s: %s", "Instruction", cpu.get_disassembly().c_str());
			ImGui::Text("%s: 0x%04X", "AF", cpu.get_af());
			ImGui::Text("%s: 0x%04X", "BC", cpu.get_bc());
			ImGui::Text("%s: 0x%04X", "DE", cpu.get_de());
			ImGui::Text("%s: 0x%04X", "HL", cpu.get_hl());
			ImGui::Value("Zero", cpu.check(LR35902::Flag::Zero));
			ImGui::Value("Negative", cpu.check(LR35902::Flag::Negative));
			ImGui::Value("HalfCarry", cpu.check(LR35902::Flag::HalfCarry));
			ImGui::Value("Carry", cpu.check(LR35902::Flag::Carry));
			ImGui::Value("Frame Cycles", static_cast<unsigned int>(cpu.frame_cycles));
		}
		if(ImGui::CollapsingHeader("GPU"))
		{
			ImGui::Text("LY: 0x%02X, LCDC: 0x%02X, STAT: 0x%02X",
				gpu.get_line(),
				gpu.get_lcdc(),
				gpu.get_lcdstat()
			);
			update_tiledata();
			update_tilemaps();
			
			float win_size = ImGui::GetWindowContentRegionMax().x;
			win_size -= ImGui::GetWindowContentRegionMin().x;
			
			ImGui::Text("Tile Data");
			float scale = std::max(0.49f * win_size / gameboy_tiledata[0].getSize().x, 1.0f);
			gameboy_tiledata_sprite[0].setScale(scale, scale);
			gameboy_tiledata_sprite[1].setScale(scale, scale);
			ImGui::Image(gameboy_tiledata_sprite[0]);
			ImGui::SameLine();
			ImGui::Image(gameboy_tiledata_sprite[1]);
			
			ImGui::Text("Tile Maps");
			scale = std::max(0.49f * win_size / gameboy_tilemap[0].getSize().x, 1.0f);
			gameboy_tilemap_sprite[0].setScale(scale, scale);
			gameboy_tilemap_sprite[1].setScale(scale, scale);
			ImGui::Image(gameboy_tilemap_sprite[0]);
			ImGui::SameLine();
			ImGui::Image(gameboy_tilemap_sprite[1]);
		}
		if(ImGui::CollapsingHeader("Breakpoints"))
		{
			static char addr_buff[32];
			ImGui::InputText("##addr", addr_buff, 32, ImGuiInputTextFlags_CharsHexadecimal);
			ImGui::SameLine();
			if(ImGui::Button("Add breakpoint") && strlen(addr_buff) > 0)
			{
				try
				{
					cpu.add_breakpoint(std::stoul(addr_buff, nullptr, 16));
				} catch(std::exception& e) {
					log("Exception: '", e.what(), "', raised while adding a breakpoint.");
				}
			}
			
			static int selected_breakpoint = 0;
			static std::vector<std::string> labels;
			ImGui::ListBox("Breakpoints", &selected_breakpoint,
			[] (void* data, int idx, const char** out_text) -> bool {
				if(labels.size() < static_cast<size_t>(idx + 1))
					labels.resize(idx + 1);
				auto addr = cpu.get_breakpoints()[idx];
				labels[idx] = Hexa(addr).str() + " (" + cpu.get_disassembly(addr) + ")";
				out_text[0] = labels[idx].c_str();
				return true;
			}, nullptr, cpu.get_breakpoints().size());
			
			if(selected_breakpoint < static_cast<int>(cpu.get_breakpoints().size()))
				if(ImGui::Button("Remove breakpoint"))
				{
					cpu.get_breakpoints().erase(cpu.get_breakpoints().begin() + selected_breakpoint);
					selected_breakpoint = 0;
				}
			
			if(ImGui::Button("Clear breakpoints"))
				clear_breakpoints();
		}
		if(ImGui::CollapsingHeader("Memory"))
		{
			ImGui::BeginChild("Memory##view", ImVec2(0,400));
			ImGuiListClipper clipper(0x10000 / 0x8);
			while(clipper.Step())
			{
				addr_t addr = clipper.DisplayStart * 0x8;
				for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
				{
					ImGui::Text("0x%04X: %02X %02X %02X %02X   %02X %02X %02X %02X", 
						addr, 
						mmu.read(addr    ), mmu.read(addr + 1), mmu.read(addr + 2), mmu.read(addr + 3), 
						mmu.read(addr + 4), mmu.read(addr + 5), mmu.read(addr + 6), mmu.read(addr + 7)
					);
					addr += 0x8;
				}
			}
			ImGui::EndChild();
		}
		if(ImGui::CollapsingHeader("Analyser (WIP)"))
		{
			if(ImGui::Button("Analyse"))
			{
				analyser.process(cartridge);
			}
			ImGui::SameLine();
			ImGui::Text("Found %d instructions, generated %d labels.", analyser.instructions.size(), analyser.get_label_count());
			if(!analyser.instructions.empty())
			{
				ImGui::BeginChild("Analysed Memory##view", ImVec2(0,400));
				ImGuiListClipper clipper(analyser.instructions.size());
				while(clipper.Step())
				{
					auto instr_it = analyser.instructions.begin();
					for(int i = 0; i < clipper.DisplayStart; ++i)
						++instr_it;
					for(int idx = clipper.DisplayStart; idx < clipper.DisplayEnd; ++idx)
					{
						addr_t addr = *instr_it;
						// If the disassembly wasn't part of the CPU class, we could
						// have some addresses replaced with the corresponding label...
						if(addr < analyser.labels.size() && analyser.labels[addr])
							ImGui::Text("%s: 0x%04X 0x%02X [%d] %s", 
								analyser.labels[addr].name.c_str(),
								addr, 
								mmu.read(addr), 
								static_cast<int>(cpu.instr_length[mmu.read(addr)]), 
								cpu.get_disassembly(addr).c_str()
							);
						else
							ImGui::Text("      0x%04X 0x%02X [%d] %s", 
								addr, 
								mmu.read(addr), 
								static_cast<int>(cpu.instr_length[mmu.read(addr)]), 
								cpu.get_disassembly(addr).c_str()
							);
						++instr_it;
					}
				}
				ImGui::EndChild();
			}
		}
		ImGui::End();
	}
    
	ImGui::Render();
}

void clear_breakpoints()
{
	cpu.clear_breakpoints();
	log("Cleared breakpoints.");
}

void advance_frame()
{
	frame_by_frame = true;
	step = true;
	debug = true;
	log("Advancing one frame");
}

void modify_volume(float v)
{
	float vol = snd_buffer.getVolume();
	vol = std::max(0.0f, std::min(vol + v, 100.0f));
	snd_buffer.setVolume(vol);
	log("Volume at ", vol);
}

void handle_event(sf::Event event)
{
	ImGui::SFML::ProcessEvent(event);

	if(event.type == sf::Event::Closed)
		window.close();
	else if(event.type == sf::Event::Resized)
		window.setView(sf::View(sf::FloatRect(0, 0, event.size.width, event.size.height)));
	else if(event.type == sf::Event::KeyPressed && 
		!(ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput))
	{
		switch(event.key.code)
		{
			case sf::Keyboard::Escape: show_gui = !show_gui; break;
			case sf::Keyboard::Space: step = true; break;
			case sf::Keyboard::Return: if(event.key.alt) toggleFullscreen(); else show_gui = !show_gui; break;
			case sf::Keyboard::BackSpace: reset(); break;
			case sf::Keyboard::Add: modify_volume(10.0f); break;
			case sf::Keyboard::Subtract: modify_volume(-10.0f); break;
			case sf::Keyboard::D: toggle_debug(); break;
			case sf::Keyboard::N: clear_breakpoints(); break;
			case sf::Keyboard::M: toggle_speed(); break;
			case sf::Keyboard::L: advance_frame(); break;
			case sf::Keyboard::P: post_process = !post_process; break;
			case sf::Keyboard::S: if(event.key.control) cartridge.save(); break;
			case sf::Keyboard::Q: if(event.key.control) window.close(); break;
			default: break;
		}
	} else if(event.type == sf::Event::JoystickButtonPressed) { // Joypad Interrupt
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
	} else if(event.type == sf::Event::JoystickButtonReleased) {
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
	log(real_speed ? "Running at real speed" : "Running as fast as possible");
}

void reset()
{
	cartridge.save();

	size_t frame_cycles = (cpu.double_speed() ? cpu.frame_cycles / 2 : cpu.frame_cycles);
	apu.end_frame(frame_cycles);
	apu.reset();
	
	snd_buffer.reset();
	cpu.reset();
	mmu.reset();
	if(rom_path.empty())
	{
		load_empty_rom();
		mmu.load_senboot();
	} else {
		if(!cartridge.load(rom_path))
		{
			log("Error loading '", rom_path, "', using an empty ROM instead.");
			load_empty_rom();
		}
		if(use_boot)
		{
			if(custom_boot)
				mmu.load_senboot();
			else
				mmu.load_boot();
		} else
			cpu.reset_cart();
	}
	gpu.reset();
	
	elapsed_cycles = 0;
	timing_clock.restart();
	frame_count = 0;
	
	if(use_movie) movie.seekg(movie_start);
	
	set_input_callbacks();
	
	log("Reset");
}
	
void load_empty_rom()
{
	// Loads a empty ROM with a valid header
	unsigned char header[0x150] = {0};
	std::memcpy(header + 0x134, "SENBOY", 6); // Title
	const unsigned char logo[3 * 16] = {
		0x00, 0x00, 0x07, 0xc7, 0x09, 0x19, 0x0f, 0x8e, 0x04, 0x67, 0x06, 0x66, 0x0f, 0xcf, 0x08, 0xd9, 
		0x0f, 0x99, 0x03, 0xb9, 0x03, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x19, 0x70, 0xdd, 0x90, 0x88, 0xf0, 
		0x54, 0x40, 0xee, 0x60, 0xcc, 0xf0, 0xdd, 0x80, 0x99, 0xf0, 0x88, 0x00, 0xcc, 0xc0, 0x00, 0x00
	};
	std::memcpy(header + 0x104, logo, 3 * 16);
	// Checksum (useless actually)
	char x = 0;
	for(int i = 0x134; i < 0x14C; ++i) x = x - header[i] - 1;
	header[0x14D] = x;
	// Busy loop to keep the logo on screen
	header[0x100] = 0xC3; // JP 0x0100
	header[0x101] = 0x00;
	header[0x102] = 0x01;
	// GBC Mode to get this sweeeeeet gbc boot ROM
	header[0x143] = 0x80;
	
	std::memcpy(header + 0x104, logo, 3 * 16);
	cartridge.load_from_memory(header, 0x150);
}

void load_movie(const char* movie_path)
{
	switch(movie_type)
	{
		case SBM:
		{
			movie.open(movie_path, std::ios::binary);
			if(movie) {
				movie_start = movie.tellg();
			}
			break;
		}
		case VBM:
			movie.open(movie_path, std::ios::binary);
			if(movie) {
				// VBM
				char tmp[4];
				movie.seekg(0x03C); // Start of the inputs, 4bytes unsigned int little endian
				movie.read(tmp, 4);
				movie_start = tmp[0] | (tmp[1] << 8) | (tmp[2] << 16) | (tmp[3] << 24);
				movie.seekg(movie_start);
			}
			break;
		case BK2: // BK2 (Not the archive, just the Input Log)
			movie.open(movie_path);
			if(movie) {
				movie.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
				movie_start = movie.tellg();
			}
			break;
		default:
			log("Error: Unknown movie type.");
			break;
	}
	if(!movie)
		log("Error: Unable to open movie file '", movie_path, "'.");
	else {
		use_movie = true;
		use_boot = false;
		log("Loaded movie file '", movie_path, "'. Starting in playback mode...");
	}
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
	if(movie_save)
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
		gameboy_tiledata[tm].update(reinterpret_cast<uint8_t*>(tile_data[tm].get()));
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
		gameboy_tilemap[tm].update(reinterpret_cast<uint8_t*>(tile_maps[tm].get()));
	}
}

void toggleFullscreen()
{
	fullscreen =! fullscreen;
	window.close();
	setup_window();
}

void setup_window()
{
	if(fullscreen)
		window.create(sf::VideoMode::getDesktopMode(), 
			"SenBoy", sf::Style::None); // Borderless Fullscreen : Way easier.
	else
		window.create(sf::VideoMode(667, 600), "SenBoy");
	window.setVerticalSyncEnabled(false);
}

void set_input_callbacks()
{
	if(!use_movie)
	{
		mmu.callback_joy_up =		[&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::Y) < -50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Up); };
		mmu.callback_joy_down =		[&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::Y) > 50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Down); };
		mmu.callback_joy_right =	[&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::X) > 50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Right); };
		mmu.callback_joy_left =		[&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::X) < -50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Left); };
		auto lambda_button = [&] (auto joy, auto key) -> bool { 
			for(int i = 0; i < sf::Joystick::Count; ++i) 
				if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, joy)) return true; 
			return sf::Keyboard::isKeyPressed(key);
		};
		mmu.callback_joy_a =		std::bind(lambda_button, 0, sf::Keyboard::F);
		mmu.callback_joy_b = 		std::bind(lambda_button, 1, sf::Keyboard::G);
		mmu.callback_joy_select =	std::bind(lambda_button, 6, sf::Keyboard::H);
		mmu.callback_joy_start =	std::bind(lambda_button, 7, sf::Keyboard::J);
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
}

void log()
{
	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	char mbstr[100];
	std::strftime(mbstr, sizeof(mbstr), "%H:%M:%S", std::localtime(&now));
	logs.push_back("[" + std::string(mbstr) + "] " + log_line.str());
	log_line.str("");
}

template<typename T, typename ...Args>
void log(const T& msg, Args... args)
{
	log_line << msg;
	log(args...);
}
