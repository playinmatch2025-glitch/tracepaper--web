# Native test of the bridge API (no browser, no emscripten needed).
#   make test
SRC := $(filter-out ../bytecode-vm/src/main.c,$(wildcard ../bytecode-vm/src/*.c))
CFLAGS ?= -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O1 -fsanitize=address,undefined

test_native: vm_api.c vm_api.h test_native.c $(SRC)
	$(CC) $(CFLAGS) -I../bytecode-vm/src -o $@ vm_api.c test_native.c $(SRC) -lm

test: test_native
	./test_native

clean:
	rm -f test_native
