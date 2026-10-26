#ifndef tp_chunk_h
#define tp_chunk_h

#include "common.h"
#include "value.h"

// One list of opcodes, expanded twice: once into the enum, once into names
// (debug.c and the bridge use the names).
#define OPCODES(X)                                                          \
  X(CONSTANT) X(NIL) X(TRUE) X(FALSE) X(POP)                                \
  X(GET_LOCAL) X(SET_LOCAL) X(GET_GLOBAL) X(DEFINE_GLOBAL) X(SET_GLOBAL)    \
  X(GET_UPVALUE) X(SET_UPVALUE)                                             \
  X(EQUAL) X(GREATER) X(LESS)                                               \
  X(ADD) X(SUBTRACT) X(MULTIPLY) X(DIVIDE) X(NOT) X(NEGATE)                 \
  X(PRINT) X(JUMP) X(JUMP_IF_FALSE) X(LOOP) X(CALL)                         \
  X(CLOSURE) X(CLOSE_UPVALUE) X(RETURN)

typedef enum {
#define X(name) OP_##name,
  OPCODES(X)
#undef X
} OpCode;

// ch.14: a chunk is a run of bytecode plus the constants it refers to.
typedef struct {
  int count;
  int capacity;
  uint8_t* code;
  int* lines;
  ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);

#endif
