#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

VM vm;
Flags flags;

// ---- output channels -------------------------------------------------------

void bufAppend(Buf* buf, const char* text, size_t len) {
  if (buf->len + len + 1 > buf->cap) {
    size_t cap = buf->cap ? buf->cap : 256;
    while (cap < buf->len + len + 1) cap *= 2;
    buf->data = (char*)realloc(buf->data, cap);
    if (buf->data == NULL) exit(1);
    buf->cap = cap;
  }
  memcpy(buf->data + buf->len, text, len);
  buf->len += len;
  buf->data[buf->len] = '\0';
}

void bufFree(Buf* buf) {
  free(buf->data);
  buf->data = NULL;
  buf->len = buf->cap = 0;
}

static void emit(Buf* buf, FILE* file, const char* fmt, va_list args) {
  if (buf == NULL) {
    vfprintf(file, fmt, args);
    return;
  }
  va_list copy;
  va_copy(copy, args);
  int n = vsnprintf(NULL, 0, fmt, copy);
  va_end(copy);
  if (n <= 0) return;
  char* tmp = (char*)malloc((size_t)n + 1);
  if (tmp == NULL) exit(1);
  vsnprintf(tmp, (size_t)n + 1, fmt, args);
  bufAppend(buf, tmp, (size_t)n);
  free(tmp);
}

void vmOut(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emit(vm.outBuf, stdout, fmt, args);
  va_end(args);
}

void vmErrV(const char* fmt, va_list args) { emit(vm.errBuf, stderr, fmt, args); }

void vmErr(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vmErrV(fmt, args);
  va_end(args);
}

// ---- stack -----------------------------------------------------------------

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vmErrV(format, args);
  va_end(args);
  vmErr("\n");

  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
    ObjFunction* function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code;
    if (instruction > 0) instruction--;  // ip already moved past the failing opcode (clamped for the step limit)
    int line = function->chunk.lines[instruction];
    if (i == vm.frameCount - 1) vm.errLine = line;
    vmErr("[line %d] in ", line);
    if (function->name == NULL) vmErr("script\n");
    else vmErr("%s()\n", function->name->chars);
  }
  resetStack();
}

// ch.24: native functions are written in C and live in the globals table.
static Value clockNative(int argCount, Value* args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void defineNative(const char* name, NativeFn function) {
  // Both objects are pushed so a collection while allocating the second
  // cannot free the first.
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

void initVM() {
  resetStack();
  vm.objects = NULL;
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;
  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;
  vm.nextObjId = 0;
  vm.gcRuns = 0;
  vm.gcFreed = 0;
  vm.steps = 0;
  vm.errLine = 0;
  initTable(&vm.globals);
  initTable(&vm.strings);
  defineNative("clock", clockNative);
}

void freeVM() {
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  freeObjects();
}

// ---- calls -----------------------------------------------------------------

static bool call(ObjClosure* closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
    return false;
  }
  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }
  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;  // the callee sits just below its arguments
  return true;
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), argCount);
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(argCount, vm.stackTop - argCount);
        vm.stackTop -= argCount + 1;
        push(result);
        return true;
      }
      default: break;  // not callable
    }
  }
  runtimeError("Can only call functions.");
  return false;
}

// ch.25: find (or make) the upvalue for a stack slot. The open list is kept
// sorted by slot so closing is a walk from the top.
static ObjUpvalue* captureUpvalue(Value* local) {
  ObjUpvalue* prevUpvalue = NULL;
  ObjUpvalue* upvalue = vm.openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }
  if (upvalue != NULL && upvalue->location == local) return upvalue;  // share it

  ObjUpvalue* createdUpvalue = newUpvalue(local);
  createdUpvalue->next = upvalue;
  if (prevUpvalue == NULL) vm.openUpvalues = createdUpvalue;
  else prevUpvalue->next = createdUpvalue;
  return createdUpvalue;
}

// Move every captured variable at or above `last` off the stack and into the heap.
static void closeUpvalues(Value* last) {
  while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
    ObjUpvalue* upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  // Both operands stay on the stack until the result exists, so a collection
  // triggered by the allocation below cannot free them.
  ObjString* b = AS_STRING(peek(0));
  ObjString* a = AS_STRING(peek(1));
  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';
  ObjString* result = takeString(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));
}

// ---- the bytecode loop, one instruction per call ---------------------------

StepResult vmStep(void) {
  CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op)                          \
  do {                                                    \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {     \
      runtimeError("Operands must be numbers.");          \
      return STEP_ERROR;                                  \
    }                                                     \
    double b = AS_NUMBER(pop());                          \
    double a = AS_NUMBER(pop());                          \
    push(valueType(a op b));                              \
  } while (false)

  if (vm.stepLimit && ++vm.steps > vm.stepLimit) {
    runtimeError("Step limit reached. Does a loop never end?");
    return STEP_ERROR;
  } else if (!vm.stepLimit) {
    vm.steps++;
  }

  if (flags.traceExecution) {
    printf("          ");
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(&frame->closure->function->chunk,
                           (int)(frame->ip - frame->closure->function->chunk.code));
  }

  uint8_t instruction;
  switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: push(READ_CONSTANT()); break;
    case OP_NIL:      push(NIL_VAL); break;
    case OP_TRUE:     push(BOOL_VAL(true)); break;
    case OP_FALSE:    push(BOOL_VAL(false)); break;
    case OP_POP:      pop(); break;

    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(frame->slots[slot]);
      break;
    }
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      frame->slots[slot] = peek(0);  // assignment is an expression: leave the value
      break;
    }
    case OP_GET_GLOBAL: {
      ObjString* name = READ_STRING();
      Value value;
      if (!tableGet(&vm.globals, name, &value)) {
        runtimeError("Undefined variable '%s'.", name->chars);
        return STEP_ERROR;
      }
      push(value);
      break;
    }
    case OP_DEFINE_GLOBAL: {
      ObjString* name = READ_STRING();
      tableSet(&vm.globals, name, peek(0));
      pop();
      break;
    }
    case OP_SET_GLOBAL: {
      ObjString* name = READ_STRING();
      if (tableSet(&vm.globals, name, peek(0))) {
        tableDelete(&vm.globals, name);  // it was new: assigning must not create it
        runtimeError("Undefined variable '%s'.", name->chars);
        return STEP_ERROR;
      }
      break;
    }
    case OP_GET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      push(*frame->closure->upvalues[slot]->location);
      break;
    }
    case OP_SET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      *frame->closure->upvalues[slot]->location = peek(0);
      break;
    }
    case OP_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(BOOL_VAL(valuesEqual(a, b)));
      break;
    }
    case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
    case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
    case OP_ADD: {
      if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
        concatenate();
      } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a + b));
      } else {
        runtimeError("Operands must be two numbers or two strings.");
        return STEP_ERROR;
      }
      break;
    }
    case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
    case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
    case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
    case OP_NOT:      push(BOOL_VAL(isFalsey(pop()))); break;
    case OP_NEGATE:
      if (!IS_NUMBER(peek(0))) {
        runtimeError("Operand must be a number.");
        return STEP_ERROR;
      }
      push(NUMBER_VAL(-AS_NUMBER(pop())));
      break;
    case OP_PRINT:
      printValue(pop());
      vmOut("\n");
      break;
    case OP_JUMP: {
      uint16_t offset = READ_SHORT();
      frame->ip += offset;
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (isFalsey(peek(0))) frame->ip += offset;
      break;
    }
    case OP_LOOP: {
      uint16_t offset = READ_SHORT();
      frame->ip -= offset;
      break;
    }
    case OP_CALL: {
      int argCount = READ_BYTE();
      if (!callValue(peek(argCount), argCount)) return STEP_ERROR;
      break;
    }
    case OP_CLOSURE: {
      ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
      ObjClosure* closure = newClosure(function);
      push(OBJ_VAL(closure));  // on the stack before we allocate upvalues (GC safety)
      for (int i = 0; i < closure->upvalueCount; i++) {
        uint8_t isLocal = READ_BYTE();
        uint8_t index = READ_BYTE();
        if (isLocal) closure->upvalues[i] = captureUpvalue(frame->slots + index);
        else closure->upvalues[i] = frame->closure->upvalues[index];
      }
      break;
    }
    case OP_CLOSE_UPVALUE:
      closeUpvalues(vm.stackTop - 1);
      pop();
      break;
    case OP_RETURN: {
      Value result = pop();
      closeUpvalues(frame->slots);
      vm.frameCount--;
      if (vm.frameCount == 0) {
        pop();  // the script closure
        return STEP_DONE;
      }
      vm.stackTop = frame->slots;
      push(result);
      break;
    }
    default:
      runtimeError("Unknown opcode %d.", instruction);
      return STEP_ERROR;
  }
  return STEP_OK;

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

bool vmStart(const char* source) {
  vm.errLine = 0;
  vm.steps = 0;
  ObjFunction* function = compile(source);
  if (function == NULL) return false;

  push(OBJ_VAL(function));  // keep it alive while the closure is allocated
  ObjClosure* closure = newClosure(function);
  pop();
  push(OBJ_VAL(closure));
  call(closure, 0);
  return true;
}

InterpretResult interpret(const char* source) {
  if (!vmStart(source)) return INTERPRET_COMPILE_ERROR;
  for (;;) {
    StepResult result = vmStep();
    if (result == STEP_DONE) return INTERPRET_OK;
    if (result == STEP_ERROR) return INTERPRET_RUNTIME_ERROR;
  }
}
