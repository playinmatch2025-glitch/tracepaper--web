# Build the clox interpreter.
#   make         optimised build
#   make asan    build with AddressSanitizer (use with --stress-gc to hunt GC bugs)
#   make test    run the shared test programs against the VM (and under stress GC)
CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wno-unused-parameter -O2
SRC     := $(wildcard src/*.c)

clox: $(SRC) $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm

asan: $(SRC) $(wildcard src/*.h)
	$(CC) -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O1 -fsanitize=address,undefined -o clox-asan $(SRC) -lm

test: clox
	../tests/run_tests.sh ./clox

clean:
	rm -f clox clox-asan
