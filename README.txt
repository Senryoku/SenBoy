# SenBoy

Work in progress GameBoy emulator.
To Do :
	* CPU Debugging
	* Interruptions
	* Sprites
	* (Sound)

## Tests
Test					| Status
------------------------|--------
01-special				| PASS
02-interrupts			| FAIL
03-op sp,hl				| PASS
04-op r,imm				| PASS
05-op rp				| PASS
06-ld r,r				| PASS
07-jr,jp,call,ret,rst	| PASS
08-misc instrs			| PASS
09-op r,r				| FAIL
10-bit ops				| FAIL
11-op a,(hl)			| FAIL