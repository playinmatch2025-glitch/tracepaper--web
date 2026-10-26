#ifndef tp_debug_h
#define tp_debug_h

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);
const char* opcodeName(uint8_t op);

#endif
