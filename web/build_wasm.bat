@echo off
set EMSDK=../../emsdk/emsdk
where /q emcc
IF ERRORLEVEL 1 (
	call %EMSDK% activate latest
)
call emcc -s WASM=1 -O3 -lidbfs.js -I ../src -I ../src/Core -I ../src/Tools -I ../ext/Gb_Snd_Emu-0.1.4/ -I ../ext/Gb_Snd_Emu-0.1.4/gb_apu -I ../ext/Gb_Snd_Emu-0.1.4/boost -std=c++20 -Wc++11-extensions ../src/Tools/Config.cpp ../src/Core/Cartridge.cpp ../src/Core/LR35902.cpp ../src/Core/MMU.cpp ../src/Core/LR35902InstrData.cpp ../src/Core/GPU.cpp  ../ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Apu.cpp ../ext/Gb_Snd_Emu-0.1.4/gb_apu/Multi_Buffer.cpp ../ext/Gb_Snd_Emu-0.1.4/gb_apu/Blip_Buffer.cpp ../ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Oscs.cpp Main.cpp --embed-file ROM -o build_wasm/index.html -s TOTAL_MEMORY=33554432 -s EXPORTED_FUNCTIONS="['_main','_load_rom','_toggle_sound','_toggle_bios','_check_save']" -s EXTRA_EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap']"
xcopy /y build_wasm\index.js www\index.js
xcopy /y build_wasm\index.wasm www\index.wasm