# SenBoy

Work in progress GameBoy emulator. It currently runs some games (Kirby's Dream Land, Tetris World, The Legend of Zelda : Link's Awakening, Pokemon Red, some public domain roms...), but only two cartridge mappers (MBC1 and MBC3) are implemented.
It is buggy, ugly and absolutely not optimised. I am not aiming for performance anyway and it's therefore pretty slow.

## Screenshots

![Kirby on SenBoy](img/SenBoy_Kirby.png)

<img src="img/SenBoy_Zelda0.png" width=300 /> <img src="img/SenBoy_Zelda1.png" width=300 /> <img src="img/SenBoy_Zelda2.png" width=300 />

## TODO
* Gameboy Color Mode
* GPU debugging
  * Check sprite priority/transparency
  * Check window position
* (Other Mappers? What popular games uses other mappers than MBC1/3?)
* (Constant coding style...)

## Tests

Blargg's cpu_instrs individual tests:

Test					| Status
------------------------|--------
01-special				| :white_check_mark: PASS
02-interrupts			| :white_check_mark: PASS
03-op sp,hl				| :white_check_mark: PASS
04-op r,imm				| :white_check_mark: PASS
05-op rp				| :white_check_mark: PASS
06-ld r,r				| :white_check_mark: PASS
07-jr,jp,call,ret,rst	| :white_check_mark: PASS
08-misc instrs			| :white_check_mark: PASS
09-op r,r				| :white_check_mark: PASS
10-bit ops				| :white_check_mark: PASS
11-op a,(hl)			| :white_check_mark: PASS
instr_timing			| :white_check_mark: PASS
01-read_timing			| :x: FAIL
02-write_timing			| :x: FAIL
03-modify_timing		| :x: FAIL

SenBoy is NOT sub-instruction accurate.

## Dependencies
* SFML (http://www.sfml-dev.org/) for graphical output and input handling.
* Gb_Snd_Emu-0.1.4 (http://blargg.8bitalley.com/libs/audio.html#Game_Music_Emu) for sound emulation (Included).

## Thanks
* http://gbdev.gg8.se/ for their awesome wiki.
* Shay 'Blargg' Green for his tests roms, his Gb_Snd_Emu library and all his contributions to the emulation scene!
