#ifndef tp_vm_h
#define tp_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

// ch.24: one CallFrame per active function call.
typedef struct {
  ObjClosure* closure;
  uint8_t* ip;     // where this call will resume
  Value* slots;    // first stack slot this call owns
} CallFrame;

typedef struct {
  char* data;
  size_t len;
  size_t cap;
} Buf;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frameCount;

  Value stack[STACK_MAX];
  Value* stackTop;
  Table globals;
  Table strings;               // the string pool (ch.20)
  ObjUpvalue* openUpvalues;

  size_t bytesAllocated;       // ch.26
  size_t nextGC;
  Obj* objects;                // every heap object, as a linked list
  int grayCount;
  int grayCapacity;
  Obj** grayStack;

  // Not in the book: counters and capture buffers for the visualizer.
  uint32_t nextObjId;
  size_t gcRuns;
  size_t gcFreed;
  size_t steps;
  size_t stepLimit;            // 0 = unlimited; the web build sets one
  Buf* outBuf;                 // when set, print output goes here
  Buf* errBuf;                 // when set, error messages go here
  int errLine;                 // line of the first error, 0 if none
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef enum { STEP_OK, STEP_DONE, STEP_ERROR } StepResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);

// Single-stepping, used by the bridge: vmStart compiles and sets up the
// first call frame; each vmStep runs exactly one bytecode instruction.
bool vmStart(const char* source);
StepResult vmStep(void);

void push(Value value);
Value pop();

void bufAppend(Buf* buf, const char* text, size_t len);
void bufFree(Buf* buf);

#endif
