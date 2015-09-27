# SenBoy

Work in progress GameBoy emulator. It currently runs some games (Kirby, Tetris, Zelda, some public domain roms...), only one cartridge mapper (MBC1) is implemented.
It is buggy, ugly and absolutely not optimised. I am not aiming for performance anyway and it's therefore pretty slow.

# Screenshots

![Kirby on SenBoy](img/SenBoy_Kirby.png)

<img src="img/SenBoy_Zelda0.png" width=300 /> <img src="img/SenBoy_Zelda1.png" width=300 /> <img src="img/SenBoy_Zelda2.png" width=300 />

## TODO
* Get Super Mario Land to work.
* Gameboy Color Mode
* Proper timing (instruction level)
* Sprite debugging
  * Handle Priority
  * Checks positions (over window?)
* (Sound? Probably not)
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
instr_timing			| :x: FAIL
01-read_timing			| :x: FAIL
02-write_timing			| :x: FAIL
03-modify_timing		| :x: FAIL

## Dependencies
* SFML (http://www.sfml-dev.org/) for graphical output and input handling.
