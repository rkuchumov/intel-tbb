CPP = g++
FLAGS = -std=c++11 
TBB_PATH = ../build/linux_intel64_gcc_cc4.8_libc2.19_kernel3.19.0_debug/
LIBS = $(TBB_PATH)libtbb_debug.so.2
INCLUDE = -I../include

.PHONY: all
all: fib custom

fib: fib.cpp
	$(CPP) $(FLAGS) $< -o $@ $(LIBS) $(INCLUDE)

custom: custom.cpp
	$(CPP) $(FLAGS) $< -o $@ $(LIBS) $(INCLUDE)

.PHONY: run
run: fib custom
	export LD_LIBRARY_PATH=$(TBB_PATH) && ./fib 25 10
	export LD_LIBRARY_PATH=$(TBB_PATH) && ./custom 6 8 10

.PHONY: clean
clean:
	rm fib || true
	rm custom
