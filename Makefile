all:
	g++ --std=c++11 -g -Wall src/Z80.cpp src/main.cpp -o SenBoy
	./SenBoy