CPP = g++
FLAGS = -std=c++11 
# LIBS = -ltbb_debug
LIBS = ../build/linux_intel64_gcc_cc4.8_libc2.19_kernel3.19.0_debug/libtbb_debug.so.2
INCLUDE = -I../include

.PHONY: all
all: fib custom

fib: fib.cpp
	$(CPP) $(FLAGS) $< -o $@ $(LIBS) $(INCLUDE)

custom: custom.cpp
	$(CPP) $(FLAGS) $< -o $@ $(LIBS) $(INCLUDE)

.PHONY: run
run: fib custom
	./fib 25 10
	./custom 6 8 10

.PHONY: clean
clean:
	rm fib || true
	rm custom
