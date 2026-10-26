#ifndef tp_common_h
#define tp_common_h

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UINT8_COUNT (UINT8_MAX + 1)

// Runtime switches. The book uses #define DEBUG_* at compile time; plain
// flags let the same binary (and the WASM build) turn them on and off.
typedef struct {
  bool printCode;       // ch.17: disassemble each function after compiling
  bool traceExecution;  // ch.15: print the stack + instruction at every step
  bool stressGC;        // ch.26: collect on EVERY allocation (finds GC bugs)
  bool logGC;           // ch.26: log allocations and collections to stderr
} Flags;
extern Flags flags;

// Output channels. `print` goes through vmOut, errors through vmErr. Both
// write into a capture buffer when one is installed (the WASM bridge does
// this), otherwise straight to stdout / stderr.
void vmOut(const char* fmt, ...);
void vmErr(const char* fmt, ...);
void vmErrV(const char* fmt, va_list args);

#endif
