CPP = g++
FLAGS = -std=c++11
TBB_PATH = ../build/linux_intel64_gcc_cc5.2.1_libc2.21_kernel4.2.0_debug/
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
	export LD_LIBRARY_PATH=$(TBB_PATH) && ./fib 20 3
	export LD_LIBRARY_PATH=$(TBB_PATH) && ./custom 6 8 10

.PHONY: clean
clean:
	rm fib || true
	rm custom
