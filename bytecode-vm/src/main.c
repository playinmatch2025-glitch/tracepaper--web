#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "vm.h"

static void repl() {
  char line[1024];
  for (;;) {
    printf("> ");
    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }
    interpret(line);
  }
}

static char* readFile(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char* buffer = (char*)malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  buffer[bytesRead] = '\0';
  fclose(file);
  return buffer;
}

static void runFile(const char* path) {
  char* source = readFile(path);
  InterpretResult result = interpret(source);
  free(source);
  if (result == INTERPRET_COMPILE_ERROR) exit(65);
  if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char* argv[]) {
  const char* path = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--trace") == 0) flags.traceExecution = true;
    else if (strcmp(argv[i], "--disasm") == 0) flags.printCode = true;
    else if (strcmp(argv[i], "--stress-gc") == 0) flags.stressGC = true;
    else if (strcmp(argv[i], "--log-gc") == 0) flags.logGC = true;
    else if (argv[i][0] == '-') {
      fprintf(stderr, "Usage: clox [--trace] [--disasm] [--stress-gc] [--log-gc] [script]\n");
      exit(64);
    } else path = argv[i];
  }

  initVM();
  if (path == NULL) repl();
  else runFile(path);
  freeVM();
  return 0;
}
