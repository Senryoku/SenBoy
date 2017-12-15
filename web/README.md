# SenBoyWeb

Port of SenBoy (https://github.com/Senryoku/SenBoy) for the web, compiled to javascript or js/wasm via emscripten.

See build and build_wasm scripts (edit the EMSDK variable with the path to your own copy of the emsdk before running them).
Combine the content of the www/ folder and the index.js / index.html.mem (for js) or index.js / index.wasm (for wasm) files from the compilation step to get a fully working SenBoyWeb copy.

Try it there:
* http://senryoku.github.io/SenBoyWeb/
* http://senryoku.github.io/SenBoyWasm/

The www/roms folder contains Public Domain roms (not my own work), referenced in the ROM file (but I can't remember the exact details tbh).
