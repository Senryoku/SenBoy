#ifdef EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include <chrono>
#include <memory>
#include <thread>
#include <SDL/SDL.h>
#include <SDL/SDL_audio.h>

#include <MMU.hpp>
#include <Cartridge.hpp>
#include <GPU.hpp>
#include <LR35902.hpp>

#include <gb_apu/Multi_Buffer.h>

// Options
bool sound = false;
bool bios = true;
size_t sample_rate = 44100;	// Audio sample rate
size_t frame_skip = 0;		// Increase for better performances.

// Components of the emulated GameBoy
Cartridge	cartridge;
MMU			mmu(cartridge);
Gb_Apu		apu;
LR35902		cpu(mmu, apu);
GPU			gpu(mmu);

bool keys[256];

Stereo_Buffer gb_snd_buffer;
const size_t chunk_size = 512;
const size_t buffer_size = chunk_size * 20;
std::mutex snd_buffer_mutex;
std::unique_ptr<blip_sample_t> snd_buffer(new blip_sample_t[buffer_size]);
size_t _offset = 0;
size_t _buff_end = 0;
size_t _buff_max = 0;
	
// Timing
std::chrono::high_resolution_clock::time_point start;
std::chrono::high_resolution_clock timing_clock;
double frame_time = 0;
size_t speed_update = 10;
double speed = 100;
uint64_t elapsed_cycles = 0;
uint64_t speed_mesure_cycles = 0;
size_t frame_count = 0;

void main_loop();
void reset();

SDL_Surface* screen;

blip_sample_t* add_samples(long int count);
void fill_audio(void *udata, Uint8 *stream, int len);

// Functions exported to JS
extern "C"
{
	int check_save()
	{
		cartridge.save();
		EM_ASM(
			FS.syncfs(false,function (err) {
				assert(!err);
				Module.print("Save folder successfully synced to local folder.");
			});
		);
		return 0;
	}

	int toggle_bios()
	{
		bios = !bios;
		return 0;
	}
	
	int toggle_sound()
	{
		sound = !sound;
		if(sound)
		{
			SDL_AudioSpec wanted;
			wanted.freq = sample_rate;
			wanted.format = AUDIO_S16SYS;
			wanted.channels = 2; 
			wanted.samples = 256;
			wanted.callback = fill_audio;
			wanted.userdata = NULL;
			if(SDL_OpenAudio(&wanted, NULL) < 0)
			{
				std::cerr << "Couldn't open audio: " << SDL_GetError() << std::endl;
				return -1;
			}
			SDL_PauseAudio(0);
			std::cout << "Sound output On" << std::endl;
		} else {
			SDL_CloseAudio();
			std::cout << "Sound output Off" << std::endl;
		}
		return 0;
	}
	
	int load_rom(const unsigned char data[], int size)
	{
		check_save();
		// Persitence folder sync is done
		if(emscripten_run_script_int("Module.syncdone") == 1)
		{
			cartridge.load_from_memory(data, size);
		}
		std::cout << "Reset" << std::endl;
		reset();
		return 0;
	}
}
		
int main()
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	screen = SDL_SetVideoMode(gpu.ScreenWidth, gpu.ScreenHeight, 32, SDL_SWSURFACE);
	std::memset(snd_buffer.get(), 0, buffer_size);
	std::memset(keys, 0, 256);
	
	EM_ASM(
		// Initialize persistent folder
		FS.mkdir('/saves');
		FS.mount(IDBFS,{},'/saves');
		Module.syncdone = 0;

		// Synchronize to Emscripten
		FS.syncfs(true, function(err) {
			assert(!err);
			Module.print("Save folder successfully synced to working folder.");
			Module.syncdone = 1;
		});
	);
	
	std::string path("ROM");	
	// Loading ROM
	if(!cartridge.load(path))
		return 0;
	
	cartridge.Log = [&] (const std::string& msg) {
		std::cout << msg << std::endl;
	};
	
	// Audio buffers
	gb_snd_buffer.clock_rate(LR35902::ClockRate);
	gb_snd_buffer.set_sample_rate(sample_rate);
	apu.output(gb_snd_buffer.center(), gb_snd_buffer.left(), gb_snd_buffer.right());

	mmu.callback_joy_up = [&] () -> bool { 
		EmscriptenGamepadEvent ge;
		int failed = emscripten_get_gamepad_status(0, &ge);
		if (!failed)
			return ge.axis[1] < -0.5 || keys[SDL_SCANCODE_UP];
		return keys[SDL_SCANCODE_UP];
	};
	mmu.callback_joy_down = [&] () -> bool { 
		EmscriptenGamepadEvent ge;
		int failed = emscripten_get_gamepad_status(0, &ge);
		if (!failed)
			return ge.axis[1] > 0.5 || keys[SDL_SCANCODE_DOWN];
		return keys[SDL_SCANCODE_DOWN];
	};
	mmu.callback_joy_right = [&] () -> bool { 
		EmscriptenGamepadEvent ge;
		int failed = emscripten_get_gamepad_status(0, &ge);
		if (!failed)
			return ge.axis[0] > 0.5 || keys[SDL_SCANCODE_RIGHT];
		return keys[SDL_SCANCODE_RIGHT];
	};
	mmu.callback_joy_left = [&] () -> bool {
		EmscriptenGamepadEvent ge;
		int failed = emscripten_get_gamepad_status(0, &ge);
		if (!failed)
			return ge.axis[0] < -0.5 || keys[SDL_SCANCODE_LEFT];
		return keys[SDL_SCANCODE_LEFT];
	};
	mmu.callback_joy_a = [&] () -> bool { 
		EmscriptenGamepadEvent ge;
		int failed = emscripten_get_gamepad_status(0, &ge);
		if (!failed)
			return ge.digitalButton[0] || keys[SDL_SCANCODE_E];
		return keys[SDL_SCANCODE_E];
	};
	mmu.callback_joy_b = [&] () -> bool {
		EmscriptenGamepadEvent ge;
		int failed = emscripten_get_gamepad_status(0, &ge);
		if (!failed)
			return ge.digitalButton[1] || keys[SDL_SCANCODE_R];
		return keys[SDL_SCANCODE_R];
	};
	mmu.callback_joy_select = [&] () -> bool {
		EmscriptenGamepadEvent ge;
		int failed = emscripten_get_gamepad_status(0, &ge);
		if (!failed)
			return ge.digitalButton[8] || keys[SDL_SCANCODE_T];
		return keys[SDL_SCANCODE_T];
	};
	mmu.callback_joy_start = [&] () -> bool {
		EmscriptenGamepadEvent ge;
		int failed = emscripten_get_gamepad_status(0, &ge);
		if (!failed)
			return ge.digitalButton[9] || keys[SDL_SCANCODE_Y];
		return keys[SDL_SCANCODE_Y];
	};
	reset();
	
	start = timing_clock.now();
	emscripten_set_main_loop(main_loop, 0, 1);
	emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
}

void main_loop()
{
	emscripten_sample_gamepad_data();
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_QUIT:
			{
				check_save();
				EM_ASM(
					FS.syncfs(false,function (err) {
						assert(!err);
					});
				);
				break;
			}
			case SDL_KEYDOWN:
				if(event.key.keysym.scancode < 256) keys[event.key.keysym.scancode] = true;
				break;
			case SDL_KEYUP:
				if(event.key.keysym.scancode < 256) keys[event.key.keysym.scancode] = false;
				break;
			default:
				break;
		}
	}

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
			
			if(gpu.completed_frame())
			{
				if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
				std::memcpy(screen->pixels, gpu.get_screen(), gpu.ScreenWidth * gpu.ScreenHeight * 4);
				if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
				SDL_Flip(screen);
			}
		} while(cpu.frame_cycles <= 70224);
		
		frame_count++;
	}
	
	size_t frame_cycles = cpu.frame_cycles;
	bool stereo = apu.end_frame(frame_cycles);
	gb_snd_buffer.end_frame(frame_cycles, stereo);
	size_t samples_count = gb_snd_buffer.samples_avail();
	if(samples_count > 0)
		samples_count = gb_snd_buffer.read_samples(add_samples(samples_count), samples_count);
}

void reset()
{
	size_t frame_cycles = (cpu.double_speed() ? cpu.frame_cycles / 2 : cpu.frame_cycles);
	apu.end_frame(frame_cycles);
	apu.reset();
	cpu.reset();
	mmu.reset();
	if(bios)
		mmu.load_senboot();
	else
		cpu.reset_cart();
	gpu.reset();
	elapsed_cycles = 0;
}

blip_sample_t* add_samples(long int count)
{
	snd_buffer_mutex.lock();
	assert(count < buffer_size);
	blip_sample_t* r = snd_buffer.get() + _buff_end;
	_buff_end += count;
	
	if(_buff_end >= buffer_size) // Too many samples at once...
		r = snd_buffer.get() + buffer_size - 1 - count;
	
	if(_buff_end > buffer_size - chunk_size * 4)
	{
		_buff_max = std::min(_buff_end, buffer_size - 1);
		_buff_end = 0;
	}
	snd_buffer_mutex.unlock();
	return r;
}

void fill_audio(void *udata, Uint8 *stream, int len)
{
	snd_buffer_mutex.lock();
	auto curr = snd_buffer.get() + _offset;
	size_t sample_size = sizeof(blip_sample_t);
	size_t max_sample_count = len / sample_size;
	if(_buff_end < _offset)
	{
		if(max_sample_count < (_buff_max - _offset))
		{
			_offset += max_sample_count;
			std::memcpy(stream, curr, max_sample_count * sample_size);
		} else {
			size_t tail = (_buff_max - _offset);
			if(max_sample_count < tail + _buff_end)
			{
				std::memcpy(stream, curr, tail * sample_size);
				
				size_t size = max_sample_count - tail;
				std::memcpy(stream + tail * sample_size, snd_buffer.get(), size * sample_size);
				_offset = size;
			}
		}
	} else {
		if(max_sample_count < _buff_end - _offset)
		{
			_offset += max_sample_count;
			std::memcpy(stream, curr, max_sample_count * sample_size);
		}
	}
	snd_buffer_mutex.unlock();
}
