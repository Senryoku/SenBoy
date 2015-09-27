#include "Cartridge.hpp"

#include <chrono>		// RTC
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>

void Cartridge::latch_clock_data()
{
	auto t = std::chrono::system_clock::now();
	time_t tt = std::chrono::system_clock::to_time_t(t);
	tm l_tm = *localtime(&tt);
	_rtc_registers[0] = l_tm.tm_sec;
	_rtc_registers[1] = l_tm.tm_min;
	_rtc_registers[2] = l_tm.tm_hour;
	size_t days = l_tm.tm_yday; //... Wep. that's wrong.
	_rtc_registers[3] = days & 0xFF;
	_rtc_registers[4] = (days & 0x1FF) >> 8;
	/// @todo Complete
}

bool Cartridge::file_exists(const std::string& path)
{
	struct stat buffer;
	return (stat (path.c_str(), &buffer) == 0);
}
