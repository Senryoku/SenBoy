#include "MMU.hpp"

#include <cstring>

MMU::MMU(Cartridge& cartridge) :
	_cartridge(&cartridge),
	_mem(new word_t[MemSize]),
	_vram_bank1(new word_t[VRAMSize])
{
	for(int i = 0; i < 8; ++i)
		_wram[i] = new word_t[WRAMSize];
	reset();
}

MMU::~MMU()
{
	delete[] _mem;
	delete[] _vram_bank1;
	for(int i = 0; i < 8; ++i)
		delete[] _wram[i];
}

void MMU::reset()
{
	std::memset(_mem, 0x00, MemSize);
	std::memset(_vram_bank1, 0x00, VRAMSize);
	for(int i = 0; i < 8; ++i)
		std::memset(_wram[i], 0x00, WRAMSize);
	for(int i = 0; i < 8; ++i)
		for(int j = 0; j < 8; ++j)
		{
			_bg_palette_data[i][j] = 0xFF;
			_sprite_palette_data[i][j] = 0xFF;
		}
	_pending_hdma = false;
}

void MMU::load_boot()
{
    if(cgb_mode())
        std::memcpy(_mem, gbc_boot, 4096);
    else
        std::memcpy(_mem, gb_boot, 256);

    write(0xFF50, word_t(0x00)); // Enable BIOS ROM
}

void MMU::load_senboot()
{
    std::memcpy(_mem, sen_boot, 256);
    write(0xFF50, word_t(0x00)); // Enable BIOS ROM
}

bool MMU::load_boot(const std::string& path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	size_t size = file.tellg();
	file.seekg(0);
	
	if(!file)
	{
		std::cerr << "Error: '" << path << "' could not be opened." << std::endl;
		return false;
	}
	
	file.read(reinterpret_cast<char*>(_mem), size);
	
	write(0xFF50, word_t(0x00)); // Enable BIOS ROM
	
	return true;
}

void MMU::update_joypad(word_t value)
{
	//assert(value == Direction || value == Button);
	_mem[P1] = ~value;
	if(_mem[P1] & Direction)
	{
		if(callback_joy_up()) _mem[P1] &= ~UpSelect;
		if(callback_joy_down()) _mem[P1] &= ~DownStart;
		if(callback_joy_left()) _mem[P1] &= ~LeftB;
		if(callback_joy_right()) _mem[P1] &= ~RightA;
	} else if(_mem[P1] & Button) {
		if(callback_joy_select()) _mem[P1] &= ~UpSelect;
		if(callback_joy_start()) _mem[P1] &= ~DownStart;
		if(callback_joy_b()) _mem[P1] &= ~LeftB;
		if(callback_joy_a()) _mem[P1] &= ~RightA;
	}
}
	
void MMU::init_dma(word_t val)
{
	_mem[DMA] = val;
	// Doing it here for now.
	// I can't find the exact timing right now.
	addr_t start = val * 0x100;
	for(addr_t i = 0; i < 0xA0; ++i)
		_mem[0xFE00 + i] = read(start + i);
}

void MMU::check_hdma()
{
	if(_pending_hdma)
	{
		_hdma_cycles = true;
		word_t length = read(HDMA5) & 0x7F;
		for(addr_t i = 0; i < 0x10; ++i)
			_hdma_dst[i] = read(_hdma_src + i);
		
		_hdma_dst += 0x10;
		_hdma_src += 0x10;
		
		if(length == 0)
		{
			_pending_hdma = false;
			_mem[HDMA5] = _mem[HDMA5] | 0x80;
		} else {				
			length--;
			_mem[HDMA5] = length;
		}
	}
}
	
void MMU::init_vram_dma(word_t val)
{
	_mem[HDMA5] = val & 0x7F;
	
	addr_t src = (read(HDMA2) + (addr_t(read(HDMA1)) << 8)) & 0xFFF0;
	addr_t dst = ((read(HDMA4) + (addr_t(read(HDMA3)) << 8)) & 0x1FF0);
	
	word_t* dest_ptr = ((_mem[VBK]) ? _vram_bank1 : _mem + 0x8000) + dst; 
	if(!(val & 0x80)) // General Purpose DMA
	{
		if(_pending_hdma) // Stop HDMA
		{
			_pending_hdma = false;
			_mem[HDMA5] = _mem[HDMA5] | 0x80;
			return;
		}
		
		word_t length = ((val & 0x7F) + 1);
		for(addr_t i = 0; i < length * 0x10; ++i)
			dest_ptr[i] = read(src + i);
		_mem[HDMA5] = 0xFF;
	} else { // H-Blank DMA
		_hdma_src = src;
		_hdma_dst = dest_ptr;
		_pending_hdma = true;
	}
}

const word_t MMU::gb_boot[256] = {
    0x31, 0xfe, 0xff, 0xaf, 0x21, 0xff, 0x9f, 0x32, 0xcb, 0x7c, 0x20, 0xfb, 0x21, 0x26, 0xff, 0x0e,
    0x11, 0x3e, 0x80, 0x32, 0xe2, 0x0c, 0x3e, 0xf3, 0xe2, 0x32, 0x3e, 0x77, 0x77, 0x3e, 0xfc, 0xe0,
    0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1a, 0xcd, 0x95, 0x00, 0xcd, 0x96, 0x00, 0x13, 0x7b,
    0xfe, 0x34, 0x20, 0xf3, 0x11, 0xd8, 0x00, 0x06, 0x08, 0x1a, 0x13, 0x22, 0x23, 0x05, 0x20, 0xf9,
    0x3e, 0x19, 0xea, 0x10, 0x99, 0x21, 0x2f, 0x99, 0x0e, 0x0c, 0x3d, 0x28, 0x08, 0x32, 0x0d, 0x20,
    0xf9, 0x2e, 0x0f, 0x18, 0xf3, 0x67, 0x3e, 0x64, 0x57, 0xe0, 0x42, 0x3e, 0x91, 0xe0, 0x40, 0x04,
    0x1e, 0x02, 0x0e, 0x0c, 0xf0, 0x44, 0xfe, 0x90, 0x20, 0xfa, 0x0d, 0x20, 0xf7, 0x1d, 0x20, 0xf2,
    0x0e, 0x13, 0x24, 0x7c, 0x1e, 0x83, 0xfe, 0x62, 0x28, 0x06, 0x1e, 0xc1, 0xfe, 0x64, 0x20, 0x06,
    0x7b, 0xe2, 0x0c, 0x3e, 0x87, 0xe2, 0xf0, 0x42, 0x90, 0xe0, 0x42, 0x15, 0x20, 0xd2, 0x05, 0x20,
    0x4f, 0x16, 0x20, 0x18, 0xcb, 0x4f, 0x06, 0x04, 0xc5, 0xcb, 0x11, 0x17, 0xc1, 0xcb, 0x11, 0x17,
    0x05, 0x20, 0xf5, 0x22, 0x23, 0x22, 0x23, 0xc9, 0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0c, 0x00, 0x0d, 0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,
    0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99, 0xbb, 0xbb, 0x67, 0x63, 0x6e, 0x0e, 0xec, 0xcc,
    0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e, 0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa5, 0x42, 0x3c,
    0x21, 0x04, 0x01, 0x11, 0xa8, 0x00, 0x1a, 0x13, 0xbe, 0x20, 0xfe, 0x23, 0x7d, 0xfe, 0x34, 0x20,
    0xf5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xfb, 0x86, 0x20, 0xfe, 0x3e, 0x01, 0xe0, 0x50
};

const word_t MMU::sen_boot[256] = {
    0x31, 0xfe, 0xff, 0xaf, 0x21, 0xff, 0x9f, 0x32, 0xcb, 0x7c, 0x20, 0xfb, 0x21, 0x26, 0xff, 0x0e,
    0x11, 0x3e, 0x80, 0x32, 0xe2, 0x0c, 0x3e, 0xf3, 0xe2, 0x32, 0x3e, 0x77, 0x77, 0x3e, 0xfc, 0xe0,
    0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1a, 0xcd, 0x95, 0x00, 0xcd, 0x96, 0x00, 0x13, 0x7b,
    0xfe, 0x34, 0x20, 0xf3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf9,
    0x3e, 0x19, 0xea, 0x10, 0x99, 0x21, 0x2f, 0x99, 0x0e, 0x0c, 0x3d, 0x28, 0x08, 0x32, 0x0d, 0x20,
    0xf9, 0x2e, 0x0f, 0x18, 0xf3, 0x67, 0x3e, 0x64, 0x57, 0xe0, 0x42, 0x3e, 0x91, 0xe0, 0x40, 0x04,
    0x1e, 0x02, 0x0e, 0x0c, 0xf0, 0x44, 0xfe, 0x90, 0x20, 0xfa, 0x0d, 0x20, 0xf7, 0x1d, 0x20, 0xf2,
    0x0e, 0x13, 0x24, 0x7c, 0x1e, 0x83, 0xfe, 0x62, 0x28, 0x06, 0x1e, 0xc1, 0xfe, 0x64, 0x20, 0x06,
    0x7b, 0xe2, 0x0c, 0x3e, 0x87, 0xe2, 0xf0, 0x42, 0x90, 0xe0, 0x42, 0x15, 0x20, 0xd2, 0x05, 0x20,
    0x4f, 0x16, 0x20, 0x18, 0xcb, 0x4f, 0x06, 0x04, 0xc5, 0xcb, 0x11, 0x17, 0xc1, 0xcb, 0x11, 0x17,
    0x05, 0x20, 0xf5, 0x22, 0x23, 0x22, 0x23, 0xc9, 0x00, 0x00, 0x07, 0xc7, 0x09, 0x19, 0x0f, 0x8e,
	0x04, 0x67, 0x06, 0x66, 0x0f, 0xcf, 0x08, 0xd9, 0x0f, 0x99, 0x03, 0xb9, 0x03, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x19, 0x70, 0xdd, 0x90, 0x88, 0xf0, 0x54, 0x40, 0xee, 0x60, 0xcc, 0xf0, 0xdd, 0x80,
	0x99, 0xf0, 0x88, 0x00, 0xcc, 0xc0, 0x00, 0x00, 0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa5, 0x42, 0x3c,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x01, 0xe0, 0x50
};

const word_t MMU::gbc_boot[4096] = {
    0x31, 0xfe, 0xff, 0x3e, 0x02, 0xc3, 0x7c, 0x00, 0xd3, 0x00, 0x98, 0xa0, 0x12, 0xd3, 0x00, 0x80,
    0x00, 0x40, 0x1e, 0x53, 0xd0, 0x00, 0x1f, 0x42, 0x1c, 0x00, 0x14, 0x2a, 0x4d, 0x19, 0x8c, 0x7e,
    0x00, 0x7c, 0x31, 0x6e, 0x4a, 0x45, 0x52, 0x4a, 0x00, 0x00, 0xff, 0x53, 0x1f, 0x7c, 0xff, 0x03,
    0x1f, 0x00, 0xff, 0x1f, 0xa7, 0x00, 0xef, 0x1b, 0x1f, 0x00, 0xef, 0x1b, 0x00, 0x7c, 0x00, 0x00,
    0xff, 0x03, 0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0c,
    0x00, 0x0d, 0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e, 0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd,
    0xd9, 0x99, 0xbb, 0xbb, 0x67, 0x63, 0x6e, 0x0e, 0xec, 0xcc, 0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9,
    0x33, 0x3e, 0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa5, 0x42, 0x3c, 0x58, 0x43, 0xe0, 0x70, 0x3e, 0xfc,
    0xe0, 0x47, 0xcd, 0x75, 0x02, 0xcd, 0x00, 0x02, 0x26, 0xd0, 0xcd, 0x03, 0x02, 0x21, 0x00, 0xfe,
    0x0e, 0xa0, 0xaf, 0x22, 0x0d, 0x20, 0xfc, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x4c, 0x1a, 0xe2,
    0x0c, 0xcd, 0xc6, 0x03, 0xcd, 0xc7, 0x03, 0x13, 0x7b, 0xfe, 0x34, 0x20, 0xf1, 0x11, 0x72, 0x00,
    0x06, 0x08, 0x1a, 0x13, 0x22, 0x23, 0x05, 0x20, 0xf9, 0xcd, 0xf0, 0x03, 0x3e, 0x01, 0xe0, 0x4f,
    0x3e, 0x91, 0xe0, 0x40, 0x21, 0xb2, 0x98, 0x06, 0x4e, 0x0e, 0x44, 0xcd, 0x91, 0x02, 0xaf, 0xe0,
    0x4f, 0x0e, 0x80, 0x21, 0x42, 0x00, 0x06, 0x18, 0xf2, 0x0c, 0xbe, 0x20, 0xfe, 0x23, 0x05, 0x20,
    0xf7, 0x21, 0x34, 0x01, 0x06, 0x19, 0x78, 0x86, 0x2c, 0x05, 0x20, 0xfb, 0x86, 0x20, 0xfe, 0xcd,
    0x1c, 0x03, 0x18, 0x02, 0x00, 0x00, 0xcd, 0xd0, 0x05, 0xaf, 0xe0, 0x70, 0x3e, 0x11, 0xe0, 0x50,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x21, 0x00, 0x80, 0xaf, 0x22, 0xcb, 0x6c, 0x28, 0xfb, 0xc9, 0x2a, 0x12, 0x13, 0x0d, 0x20, 0xfa,
    0xc9, 0xe5, 0x21, 0x0f, 0xff, 0xcb, 0x86, 0xcb, 0x46, 0x28, 0xfc, 0xe1, 0xc9, 0x11, 0x00, 0xff,
    0x21, 0x03, 0xd0, 0x0e, 0x0f, 0x3e, 0x30, 0x12, 0x3e, 0x20, 0x12, 0x1a, 0x2f, 0xa1, 0xcb, 0x37,
    0x47, 0x3e, 0x10, 0x12, 0x1a, 0x2f, 0xa1, 0xb0, 0x4f, 0x7e, 0xa9, 0xe6, 0xf0, 0x47, 0x2a, 0xa9,
    0xa1, 0xb0, 0x32, 0x47, 0x79, 0x77, 0x3e, 0x30, 0x12, 0xc9, 0x3e, 0x80, 0xe0, 0x68, 0xe0, 0x6a,
    0x0e, 0x6b, 0x2a, 0xe2, 0x05, 0x20, 0xfb, 0x4a, 0x09, 0x43, 0x0e, 0x69, 0x2a, 0xe2, 0x05, 0x20,
    0xfb, 0xc9, 0xc5, 0xd5, 0xe5, 0x21, 0x00, 0xd8, 0x06, 0x01, 0x16, 0x3f, 0x1e, 0x40, 0xcd, 0x4a,
    0x02, 0xe1, 0xd1, 0xc1, 0xc9, 0x3e, 0x80, 0xe0, 0x26, 0xe0, 0x11, 0x3e, 0xf3, 0xe0, 0x12, 0xe0,
    0x25, 0x3e, 0x77, 0xe0, 0x24, 0x21, 0x30, 0xff, 0xaf, 0x0e, 0x10, 0x22, 0x2f, 0x0d, 0x20, 0xfb,
    0xc9, 0xcd, 0x11, 0x02, 0xcd, 0x62, 0x02, 0x79, 0xfe, 0x38, 0x20, 0x14, 0xe5, 0xaf, 0xe0, 0x4f,
    0x21, 0xa7, 0x99, 0x3e, 0x38, 0x22, 0x3c, 0xfe, 0x3f, 0x20, 0xfa, 0x3e, 0x01, 0xe0, 0x4f, 0xe1,
    0xc5, 0xe5, 0x21, 0x43, 0x01, 0xcb, 0x7e, 0xcc, 0x89, 0x05, 0xe1, 0xc1, 0xcd, 0x11, 0x02, 0x79,
    0xd6, 0x30, 0xd2, 0x06, 0x03, 0x79, 0xfe, 0x01, 0xca, 0x06, 0x03, 0x7d, 0xfe, 0xd1, 0x28, 0x21,
    0xc5, 0x06, 0x03, 0x0e, 0x01, 0x16, 0x03, 0x7e, 0xe6, 0xf8, 0xb1, 0x22, 0x15, 0x20, 0xf8, 0x0c,
    0x79, 0xfe, 0x06, 0x20, 0xf0, 0x11, 0x11, 0x00, 0x19, 0x05, 0x20, 0xe7, 0x11, 0xa1, 0xff, 0x19,
    0xc1, 0x04, 0x78, 0x1e, 0x83, 0xfe, 0x62, 0x28, 0x06, 0x1e, 0xc1, 0xfe, 0x64, 0x20, 0x07, 0x7b,
    0xe0, 0x13, 0x3e, 0x87, 0xe0, 0x14, 0xfa, 0x02, 0xd0, 0xfe, 0x00, 0x28, 0x0a, 0x3d, 0xea, 0x02,
    0xd0, 0x79, 0xfe, 0x01, 0xca, 0x91, 0x02, 0x0d, 0xc2, 0x91, 0x02, 0xc9, 0x0e, 0x26, 0xcd, 0x4a,
    0x03, 0xcd, 0x11, 0x02, 0xcd, 0x62, 0x02, 0x0d, 0x20, 0xf4, 0xcd, 0x11, 0x02, 0x3e, 0x01, 0xe0,
    0x4f, 0xcd, 0x3e, 0x03, 0xcd, 0x41, 0x03, 0xaf, 0xe0, 0x4f, 0xcd, 0x3e, 0x03, 0xc9, 0x21, 0x08,
    0x00, 0x11, 0x51, 0xff, 0x0e, 0x05, 0xcd, 0x0a, 0x02, 0xc9, 0xc5, 0xd5, 0xe5, 0x21, 0x40, 0xd8,
    0x0e, 0x20, 0x7e, 0xe6, 0x1f, 0xfe, 0x1f, 0x28, 0x01, 0x3c, 0x57, 0x2a, 0x07, 0x07, 0x07, 0xe6,
    0x07, 0x47, 0x3a, 0x07, 0x07, 0x07, 0xe6, 0x18, 0xb0, 0xfe, 0x1f, 0x28, 0x01, 0x3c, 0x0f, 0x0f,
    0x0f, 0x47, 0xe6, 0xe0, 0xb2, 0x22, 0x78, 0xe6, 0x03, 0x5f, 0x7e, 0x0f, 0x0f, 0xe6, 0x1f, 0xfe,
    0x1f, 0x28, 0x01, 0x3c, 0x07, 0x07, 0xb3, 0x22, 0x0d, 0x20, 0xc7, 0xe1, 0xd1, 0xc1, 0xc9, 0x0e,
    0x00, 0x1a, 0xe6, 0xf0, 0xcb, 0x49, 0x28, 0x02, 0xcb, 0x37, 0x47, 0x23, 0x7e, 0xb0, 0x22, 0x1a,
    0xe6, 0x0f, 0xcb, 0x49, 0x20, 0x02, 0xcb, 0x37, 0x47, 0x23, 0x7e, 0xb0, 0x22, 0x13, 0xcb, 0x41,
    0x28, 0x0d, 0xd5, 0x11, 0xf8, 0xff, 0xcb, 0x49, 0x28, 0x03, 0x11, 0x08, 0x00, 0x19, 0xd1, 0x0c,
    0x79, 0xfe, 0x18, 0x20, 0xcc, 0xc9, 0x47, 0xd5, 0x16, 0x04, 0x58, 0xcb, 0x10, 0x17, 0xcb, 0x13,
    0x17, 0x15, 0x20, 0xf6, 0xd1, 0x22, 0x23, 0x22, 0x23, 0xc9, 0x3e, 0x19, 0xea, 0x10, 0x99, 0x21,
    0x2f, 0x99, 0x0e, 0x0c, 0x3d, 0x28, 0x08, 0x32, 0x0d, 0x20, 0xf9, 0x2e, 0x0f, 0x18, 0xf3, 0xc9,
    0x3e, 0x01, 0xe0, 0x4f, 0xcd, 0x00, 0x02, 0x11, 0x07, 0x06, 0x21, 0x80, 0x80, 0x0e, 0xc0, 0x1a,
    0x22, 0x23, 0x22, 0x23, 0x13, 0x0d, 0x20, 0xf7, 0x11, 0x04, 0x01, 0xcd, 0x8f, 0x03, 0x01, 0xa8,
    0xff, 0x09, 0xcd, 0x8f, 0x03, 0x01, 0xf8, 0xff, 0x09, 0x11, 0x72, 0x00, 0x0e, 0x08, 0x23, 0x1a,
    0x22, 0x13, 0x0d, 0x20, 0xf9, 0x21, 0xc2, 0x98, 0x06, 0x08, 0x3e, 0x08, 0x0e, 0x10, 0x22, 0x0d,
    0x20, 0xfc, 0x11, 0x10, 0x00, 0x19, 0x05, 0x20, 0xf3, 0xaf, 0xe0, 0x4f, 0x21, 0xc2, 0x98, 0x3e,
    0x08, 0x22, 0x3c, 0xfe, 0x18, 0x20, 0x02, 0x2e, 0xe2, 0xfe, 0x28, 0x20, 0x03, 0x21, 0x02, 0x99,
    0xfe, 0x38, 0x20, 0xed, 0x21, 0xd8, 0x08, 0x11, 0x40, 0xd8, 0x06, 0x08, 0x3e, 0xff, 0x12, 0x13,
    0x12, 0x13, 0x0e, 0x02, 0xcd, 0x0a, 0x02, 0x3e, 0x00, 0x12, 0x13, 0x12, 0x13, 0x13, 0x13, 0x05,
    0x20, 0xea, 0xcd, 0x62, 0x02, 0x21, 0x4b, 0x01, 0x7e, 0xfe, 0x33, 0x20, 0x0b, 0x2e, 0x44, 0x1e,
    0x30, 0x2a, 0xbb, 0x20, 0x49, 0x1c, 0x18, 0x04, 0x2e, 0x4b, 0x1e, 0x01, 0x2a, 0xbb, 0x20, 0x3e,
    0x2e, 0x34, 0x01, 0x10, 0x00, 0x2a, 0x80, 0x47, 0x0d, 0x20, 0xfa, 0xea, 0x00, 0xd0, 0x21, 0xc7,
    0x06, 0x0e, 0x00, 0x2a, 0xb8, 0x28, 0x08, 0x0c, 0x79, 0xfe, 0x4f, 0x20, 0xf6, 0x18, 0x1f, 0x79,
    0xd6, 0x41, 0x38, 0x1c, 0x21, 0x16, 0x07, 0x16, 0x00, 0x5f, 0x19, 0xfa, 0x37, 0x01, 0x57, 0x7e,
    0xba, 0x28, 0x0d, 0x11, 0x0e, 0x00, 0x19, 0x79, 0x83, 0x4f, 0xd6, 0x5e, 0x38, 0xed, 0x0e, 0x00,
    0x21, 0x33, 0x07, 0x06, 0x00, 0x09, 0x7e, 0xe6, 0x1f, 0xea, 0x08, 0xd0, 0x7e, 0xe6, 0xe0, 0x07,
    0x07, 0x07, 0xea, 0x0b, 0xd0, 0xcd, 0xe9, 0x04, 0xc9, 0x11, 0x91, 0x07, 0x21, 0x00, 0xd9, 0xfa,
    0x0b, 0xd0, 0x47, 0x0e, 0x1e, 0xcb, 0x40, 0x20, 0x02, 0x13, 0x13, 0x1a, 0x22, 0x20, 0x02, 0x1b,
    0x1b, 0xcb, 0x48, 0x20, 0x02, 0x13, 0x13, 0x1a, 0x22, 0x13, 0x13, 0x20, 0x02, 0x1b, 0x1b, 0xcb,
    0x50, 0x28, 0x05, 0x1b, 0x2b, 0x1a, 0x22, 0x13, 0x1a, 0x22, 0x13, 0x0d, 0x20, 0xd7, 0x21, 0x00,
    0xd9, 0x11, 0x00, 0xda, 0xcd, 0x64, 0x05, 0xc9, 0x21, 0x12, 0x00, 0xfa, 0x05, 0xd0, 0x07, 0x07,
    0x06, 0x00, 0x4f, 0x09, 0x11, 0x40, 0xd8, 0x06, 0x08, 0xe5, 0x0e, 0x02, 0xcd, 0x0a, 0x02, 0x13,
    0x13, 0x13, 0x13, 0x13, 0x13, 0xe1, 0x05, 0x20, 0xf0, 0x11, 0x42, 0xd8, 0x0e, 0x02, 0xcd, 0x0a,
    0x02, 0x11, 0x4a, 0xd8, 0x0e, 0x02, 0xcd, 0x0a, 0x02, 0x2b, 0x2b, 0x11, 0x44, 0xd8, 0x0e, 0x02,
    0xcd, 0x0a, 0x02, 0xc9, 0x0e, 0x60, 0x2a, 0xe5, 0xc5, 0x21, 0xe8, 0x07, 0x06, 0x00, 0x4f, 0x09,
    0x0e, 0x08, 0xcd, 0x0a, 0x02, 0xc1, 0xe1, 0x0d, 0x20, 0xec, 0xc9, 0xfa, 0x08, 0xd0, 0x11, 0x18,
    0x00, 0x3c, 0x3d, 0x28, 0x03, 0x19, 0x20, 0xfa, 0xc9, 0xcd, 0x1d, 0x02, 0x78, 0xe6, 0xff, 0x28,
    0x0f, 0x21, 0xe4, 0x08, 0x06, 0x00, 0x2a, 0xb9, 0x28, 0x08, 0x04, 0x78, 0xfe, 0x0c, 0x20, 0xf6,
    0x18, 0x2d, 0x78, 0xea, 0x05, 0xd0, 0x3e, 0x1e, 0xea, 0x02, 0xd0, 0x11, 0x0b, 0x00, 0x19, 0x56,
    0x7a, 0xe6, 0x1f, 0x5f, 0x21, 0x08, 0xd0, 0x3a, 0x22, 0x7b, 0x77, 0x7a, 0xe6, 0xe0, 0x07, 0x07,
    0x07, 0x5f, 0x21, 0x0b, 0xd0, 0x3a, 0x22, 0x7b, 0x77, 0xcd, 0xe9, 0x04, 0xcd, 0x28, 0x05, 0xc9,
    0xcd, 0x11, 0x02, 0xfa, 0x43, 0x01, 0xcb, 0x7f, 0x28, 0x04, 0xe0, 0x4c, 0x18, 0x28, 0x3e, 0x04,
    0xe0, 0x4c, 0x3e, 0x01, 0xe0, 0x6c, 0x21, 0x00, 0xda, 0xcd, 0x7b, 0x05, 0x06, 0x10, 0x16, 0x00,
    0x1e, 0x08, 0xcd, 0x4a, 0x02, 0x21, 0x7a, 0x00, 0xfa, 0x00, 0xd0, 0x47, 0x0e, 0x02, 0x2a, 0xb8,
    0xcc, 0xda, 0x03, 0x0d, 0x20, 0xf8, 0xc9, 0x01, 0x0f, 0x3f, 0x7e, 0xff, 0xff, 0xc0, 0x00, 0xc0,
    0xf0, 0xf1, 0x03, 0x7c, 0xfc, 0xfe, 0xfe, 0x03, 0x07, 0x07, 0x0f, 0xe0, 0xe0, 0xf0, 0xf0, 0x1e,
    0x3e, 0x7e, 0xfe, 0x0f, 0x0f, 0x1f, 0x1f, 0xff, 0xff, 0x00, 0x00, 0x01, 0x01, 0x01, 0x03, 0xff,
    0xff, 0xe1, 0xe0, 0xc0, 0xf0, 0xf9, 0xfb, 0x1f, 0x7f, 0xf8, 0xe0, 0xf3, 0xfd, 0x3e, 0x1e, 0xe0,
    0xf0, 0xf9, 0x7f, 0x3e, 0x7c, 0xf8, 0xe0, 0xf8, 0xf0, 0xf0, 0xf8, 0x00, 0x00, 0x7f, 0x7f, 0x07,
    0x0f, 0x9f, 0xbf, 0x9e, 0x1f, 0xff, 0xff, 0x0f, 0x1e, 0x3e, 0x3c, 0xf1, 0xfb, 0x7f, 0x7f, 0xfe,
    0xde, 0xdf, 0x9f, 0x1f, 0x3f, 0x3e, 0x3c, 0xf8, 0xf8, 0x00, 0x00, 0x03, 0x03, 0x07, 0x07, 0xff,
    0xff, 0xc1, 0xc0, 0xf3, 0xe7, 0xf7, 0xf3, 0xc0, 0xc0, 0xc0, 0xc0, 0x1f, 0x1f, 0x1e, 0x3e, 0x3f,
    0x1f, 0x3e, 0x3e, 0x80, 0x00, 0x00, 0x00, 0x7c, 0x1f, 0x07, 0x00, 0x0f, 0xff, 0xfe, 0x00, 0x7c,
    0xf8, 0xf0, 0x00, 0x1f, 0x0f, 0x0f, 0x00, 0x7c, 0xf8, 0xf8, 0x00, 0x3f, 0x3e, 0x1c, 0x00, 0x0f,
    0x0f, 0x0f, 0x00, 0x7c, 0xff, 0xff, 0x00, 0x00, 0xf8, 0xf8, 0x00, 0x07, 0x0f, 0x0f, 0x00, 0x81,
    0xff, 0xff, 0x00, 0xf3, 0xe1, 0x80, 0x00, 0xe0, 0xff, 0x7f, 0x00, 0xfc, 0xf0, 0xc0, 0x00, 0x3e,
    0x7c, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x16, 0x36, 0xd1, 0xdb, 0xf2, 0x3c, 0x8c,
    0x92, 0x3d, 0x5c, 0x58, 0xc9, 0x3e, 0x70, 0x1d, 0x59, 0x69, 0x19, 0x35, 0xa8, 0x14, 0xaa, 0x75,
    0x95, 0x99, 0x34, 0x6f, 0x15, 0xff, 0x97, 0x4b, 0x90, 0x17, 0x10, 0x39, 0xf7, 0xf6, 0xa2, 0x49,
    0x4e, 0x43, 0x68, 0xe0, 0x8b, 0xf0, 0xce, 0x0c, 0x29, 0xe8, 0xb7, 0x86, 0x9a, 0x52, 0x01, 0x9d,
    0x71, 0x9c, 0xbd, 0x5d, 0x6d, 0x67, 0x3f, 0x6b, 0xb3, 0x46, 0x28, 0xa5, 0xc6, 0xd3, 0x27, 0x61,
    0x18, 0x66, 0x6a, 0xbf, 0x0d, 0xf4, 0x42, 0x45, 0x46, 0x41, 0x41, 0x52, 0x42, 0x45, 0x4b, 0x45,
    0x4b, 0x20, 0x52, 0x2d, 0x55, 0x52, 0x41, 0x52, 0x20, 0x49, 0x4e, 0x41, 0x49, 0x4c, 0x49, 0x43,
    0x45, 0x20, 0x52, 0x7c, 0x08, 0x12, 0xa3, 0xa2, 0x07, 0x87, 0x4b, 0x20, 0x12, 0x65, 0xa8, 0x16,
    0xa9, 0x86, 0xb1, 0x68, 0xa0, 0x87, 0x66, 0x12, 0xa1, 0x30, 0x3c, 0x12, 0x85, 0x12, 0x64, 0x1b,
    0x07, 0x06, 0x6f, 0x6e, 0x6e, 0xae, 0xaf, 0x6f, 0xb2, 0xaf, 0xb2, 0xa8, 0xab, 0x6f, 0xaf, 0x86,
    0xae, 0xa2, 0xa2, 0x12, 0xaf, 0x13, 0x12, 0xa1, 0x6e, 0xaf, 0xaf, 0xad, 0x06, 0x4c, 0x6e, 0xaf,
    0xaf, 0x12, 0x7c, 0xac, 0xa8, 0x6a, 0x6e, 0x13, 0xa0, 0x2d, 0xa8, 0x2b, 0xac, 0x64, 0xac, 0x6d,
    0x87, 0xbc, 0x60, 0xb4, 0x13, 0x72, 0x7c, 0xb5, 0xae, 0xae, 0x7c, 0x7c, 0x65, 0xa2, 0x6c, 0x64,
    0x85, 0x80, 0xb0, 0x40, 0x88, 0x20, 0x68, 0xde, 0x00, 0x70, 0xde, 0x20, 0x78, 0x20, 0x20, 0x38,
    0x20, 0xb0, 0x90, 0x20, 0xb0, 0xa0, 0xe0, 0xb0, 0xc0, 0x98, 0xb6, 0x48, 0x80, 0xe0, 0x50, 0x1e,
    0x1e, 0x58, 0x20, 0xb8, 0xe0, 0x88, 0xb0, 0x10, 0x20, 0x00, 0x10, 0x20, 0xe0, 0x18, 0xe0, 0x18,
    0x00, 0x18, 0xe0, 0x20, 0xa8, 0xe0, 0x20, 0x18, 0xe0, 0x00, 0x20, 0x18, 0xd8, 0xc8, 0x18, 0xe0,
    0x00, 0xe0, 0x40, 0x28, 0x28, 0x28, 0x18, 0xe0, 0x60, 0x20, 0x18, 0xe0, 0x00, 0x00, 0x08, 0xe0,
    0x18, 0x30, 0xd0, 0xd0, 0xd0, 0x20, 0xe0, 0xe8, 0xff, 0x7f, 0xbf, 0x32, 0xd0, 0x00, 0x00, 0x00,
    0x9f, 0x63, 0x79, 0x42, 0xb0, 0x15, 0xcb, 0x04, 0xff, 0x7f, 0x31, 0x6e, 0x4a, 0x45, 0x00, 0x00,
    0xff, 0x7f, 0xef, 0x1b, 0x00, 0x02, 0x00, 0x00, 0xff, 0x7f, 0x1f, 0x42, 0xf2, 0x1c, 0x00, 0x00,
    0xff, 0x7f, 0x94, 0x52, 0x4a, 0x29, 0x00, 0x00, 0xff, 0x7f, 0xff, 0x03, 0x2f, 0x01, 0x00, 0x00,
    0xff, 0x7f, 0xef, 0x03, 0xd6, 0x01, 0x00, 0x00, 0xff, 0x7f, 0xb5, 0x42, 0xc8, 0x3d, 0x00, 0x00,
    0x74, 0x7e, 0xff, 0x03, 0x80, 0x01, 0x00, 0x00, 0xff, 0x67, 0xac, 0x77, 0x13, 0x1a, 0x6b, 0x2d,
    0xd6, 0x7e, 0xff, 0x4b, 0x75, 0x21, 0x00, 0x00, 0xff, 0x53, 0x5f, 0x4a, 0x52, 0x7e, 0x00, 0x00,
    0xff, 0x4f, 0xd2, 0x7e, 0x4c, 0x3a, 0xe0, 0x1c, 0xed, 0x03, 0xff, 0x7f, 0x5f, 0x25, 0x00, 0x00,
    0x6a, 0x03, 0x1f, 0x02, 0xff, 0x03, 0xff, 0x7f, 0xff, 0x7f, 0xdf, 0x01, 0x12, 0x01, 0x00, 0x00,
    0x1f, 0x23, 0x5f, 0x03, 0xf2, 0x00, 0x09, 0x00, 0xff, 0x7f, 0xea, 0x03, 0x1f, 0x01, 0x00, 0x00,
    0x9f, 0x29, 0x1a, 0x00, 0x0c, 0x00, 0x00, 0x00, 0xff, 0x7f, 0x7f, 0x02, 0x1f, 0x00, 0x00, 0x00,
    0xff, 0x7f, 0xe0, 0x03, 0x06, 0x02, 0x20, 0x01, 0xff, 0x7f, 0xeb, 0x7e, 0x1f, 0x00, 0x00, 0x7c,
    0xff, 0x7f, 0xff, 0x3f, 0x00, 0x7e, 0x1f, 0x00, 0xff, 0x7f, 0xff, 0x03, 0x1f, 0x00, 0x00, 0x00,
    0xff, 0x03, 0x1f, 0x00, 0x0c, 0x00, 0x00, 0x00, 0xff, 0x7f, 0x3f, 0x03, 0x93, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x42, 0x7f, 0x03, 0xff, 0x7f, 0xff, 0x7f, 0x8c, 0x7e, 0x00, 0x7c, 0x00, 0x00,
    0xff, 0x7f, 0xef, 0x1b, 0x80, 0x61, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x7c, 0xe0, 0x03, 0x1f, 0x7c,
    0x1f, 0x00, 0xff, 0x03, 0x40, 0x41, 0x42, 0x20, 0x21, 0x22, 0x80, 0x81, 0x82, 0x10, 0x11, 0x12,
    0x12, 0xb0, 0x79, 0xb8, 0xad, 0x16, 0x17, 0x07, 0xba, 0x05, 0x7c, 0x13, 0x00, 0x00, 0x00, 0x00
};
