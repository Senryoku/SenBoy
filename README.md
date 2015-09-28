# SenBoy

Work in progress GameBoy emulator. It currently runs some games (Kirby's Dream Land, Tetris World, The Legend of Zelda : Link's Awakening, Pokemon Red, some public domain roms...), but only two cartridge mappers (MBC1 and MBC3) are implemented.
It is buggy, ugly and absolutely not optimised. I am not aiming for performance anyway and it's therefore pretty slow.

## Screenshots

![Kirby on SenBoy](img/SenBoy_Kirby.png)

<img src="img/SenBoy_Zelda0.png" width=300 /> <img src="img/SenBoy_Zelda1.png" width=300 /> <img src="img/SenBoy_Zelda2.png" width=300 />

## TODO
* Gameboy Color Mode
* Sprite debugging
  * Check priority
* (Other Mappers? What popular games uses other mappers than MBC1/3?)
* (Constant coding style...)

## Tests

Blargg's cpu_instrs individual tests:

Test					| Status
------------------------|--------
01-special				| :white_check_mark: PASS
02-interrupts			| :white_check_mark: PASS
03-op sp,hl				| :x: FAIL *
04-op r,imm				| :white_check_mark: PASS
05-op rp				| :white_check_mark: PASS
06-ld r,r				| :white_check_mark: PASS
07-jr,jp,call,ret,rst	| :x: FAIL *
08-misc instrs			| :x: FAIL *
09-op r,r				| :white_check_mark: PASS
10-bit ops				| :white_check_mark: PASS
11-op a,(hl)			| :white_check_mark: PASS
instr_timing			| Regression: FAIL since [this commit](https://github.com/Senryoku/SenBoy/commit/b50b373a70784d5c63ce9cdcdcebbaca88c7ae36)
01-read_timing			| :x: FAIL
02-write_timing			| :x: FAIL
03-modify_timing		| :x: FAIL

*: These are a regression since [this commit](https://github.com/Senryoku/SenBoy/commit/b50b373a70784d5c63ce9cdcdcebbaca88c7ae36) (and before [this one](https://github.com/Senryoku/SenBoy/commit/124faa1eda687f691fb4098ff53931e5547dcc76)).

SenBoy is NOT sub-instruction accurate.

## Dependencies
* SFML (http://www.sfml-dev.org/) for graphical output and input handling.
* Gb_Snd_Emu-0.1.4 (http://blargg.8bitalley.com/libs/audio.html#Game_Music_Emu) for sound emulation (Included).

## Thanks
* http://gbdev.gg8.se/ for their awesome wiki.
* Shay 'Blargg' Green for his tests roms, his Gb_Snd_Emu library and all his contributions to the emulation scene!
