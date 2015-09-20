#include "GPU.hpp"

void GPU::reset()
{
	std::memset(_vram, 0, 0x1FFF);
	std::memset(_oam, 0, 0x00A0);
	std::memset(screen, 0xFF, ScreenWidth * ScreenHeight * sizeof(color_t));
	
	getLine() = getScrollX() = getScrollY() = getBGPalette() = getLCDControl() = getLCDStatus() = _cycles = 0;
	getLCDStatus() = (getLCDStatus() & ~LCDMode) | Mode::OAM;
}
