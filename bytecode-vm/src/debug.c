#include <stdio.h>
#include "debug.h"
#include "object.h"
#include "value.h"

static const char* opNames[] = {
#define X(name) "OP_" #name,
  OPCODES(X)
#undef X
};

const char* opcodeName(uint8_t op) {
  return op < sizeof(opNames) / sizeof(opNames[0]) ? opNames[op] : "OP_UNKNOWN";
}

void disassembleChunk(Chunk* chunk, const char* name) {
  printf("== %s ==\n", name);
  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static int simpleInstruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset) {
  printf("%-16s %4d\n", name, chunk->code[offset + 1]);
  return offset + 2;
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 2;
}

int disassembleInstruction(Chunk* chunk, int offset) {
  printf("%04d ", offset);
  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) printf("   | ");
  else printf("%4d ", chunk->lines[offset]);

  uint8_t instruction = chunk->code[offset];
  const char* name = opcodeName(instruction);
  switch (instruction) {
    case OP_CONSTANT: case OP_GET_GLOBAL: case OP_DEFINE_GLOBAL: case OP_SET_GLOBAL:
      return constantInstruction(name, chunk, offset);
    case OP_GET_LOCAL: case OP_SET_LOCAL: case OP_GET_UPVALUE: case OP_SET_UPVALUE: case OP_CALL:
      return byteInstruction(name, chunk, offset);
    case OP_JUMP: case OP_JUMP_IF_FALSE:
      return jumpInstruction(name, 1, chunk, offset);
    case OP_LOOP:
      return jumpInstruction(name, -1, chunk, offset);
    case OP_CLOSURE: {
      offset++;
      uint8_t constant = chunk->code[offset++];
      printf("%-16s %4d ", name, constant);
      printValue(chunk->constants.values[constant]);
      printf("\n");
      ObjFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
      for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04d      |                     %s %d\n", offset - 2, isLocal ? "local" : "upvalue", index);
      }
      return offset;
    }
    default:
      if (instruction < sizeof(opNames) / sizeof(opNames[0])) return simpleInstruction(name, offset);
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}
