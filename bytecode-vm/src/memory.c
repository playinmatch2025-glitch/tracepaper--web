#include "memory.h"
#include "compiler.h"
#include "vm.h"

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  vm.bytesAllocated += newSize - oldSize;

  if (newSize > oldSize) {
    if (flags.stressGC) collectGarbage();                    // torture mode
    if (vm.bytesAllocated > vm.nextGC) collectGarbage();     // normal trigger
  }

  if (newSize == 0) {
    free(pointer);
    return NULL;
  }
  void* result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}

// ---- Phase 1: marking ------------------------------------------------------
// "Gray" objects are reached but their children are not yet visited.

void markObject(Obj* object) {
  if (object == NULL) return;
  if (object->isMarked) return;
  if (flags.logGC) {
    fprintf(stderr, "%p mark ", (void*)object);
    fflush(stderr);
  }
  object->isMarked = true;

  if (vm.grayCapacity < vm.grayCount + 1) {
    vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
    // The gray stack uses plain realloc: asking the collector's own
    // allocator for room *during* a collection would recurse.
    vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
    if (vm.grayStack == NULL) exit(1);
  }
  vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
  if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void markArray(ValueArray* array) {
  for (int i = 0; i < array->count; i++) markValue(array->values[i]);
}

// "Blacken": mark everything this object points at.
static void blackenObject(Obj* object) {
  if (flags.logGC) fprintf(stderr, "%p blacken\n", (void*)object);
  switch (object->type) {
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      markObject((Obj*)closure->function);
      for (int i = 0; i < closure->upvalueCount; i++) markObject((Obj*)closure->upvalues[i]);
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      markObject((Obj*)function->name);
      markArray(&function->chunk.constants);
      break;
    }
    case OBJ_UPVALUE:
      markValue(((ObjUpvalue*)object)->closed);
      break;
    case OBJ_NATIVE:
    case OBJ_STRING:
      break;  // no outgoing references
  }
}

static void freeObject(Obj* object) {
  if (flags.logGC) fprintf(stderr, "%p free type %s\n", (void*)object, objTypeName(object->type));
  switch (object->type) {
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
      FREE(ObjClosure, object);
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      freeChunk(&function->chunk);
      FREE(ObjFunction, object);
      break;
    }
    case OBJ_NATIVE:
      FREE(ObjNative, object);
      break;
    case OBJ_STRING: {
      ObjString* string = (ObjString*)object;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjString, object);
      break;
    }
    case OBJ_UPVALUE:
      FREE(ObjUpvalue, object);
      break;
  }
}

// The roots: everything the running program can reach without following
// another object. If it is not reachable from here, it is garbage.
static void markRoots() {
  for (Value* slot = vm.stack; slot < vm.stackTop; slot++) markValue(*slot);
  for (int i = 0; i < vm.frameCount; i++) markObject((Obj*)vm.frames[i].closure);
  for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
    markObject((Obj*)upvalue);
  }
  markTable(&vm.globals);
  markCompilerRoots();  // functions still being compiled are not on the stack yet
}

static void traceReferences() {
  while (vm.grayCount > 0) {
    Obj* object = vm.grayStack[--vm.grayCount];
    blackenObject(object);
  }
}

// ---- Phase 2: sweeping -----------------------------------------------------

static void sweep() {
  Obj* previous = NULL;
  Obj* object = vm.objects;
  while (object != NULL) {
    if (object->isMarked) {
      object->isMarked = false;  // reset for the next collection
      previous = object;
      object = object->next;
    } else {
      Obj* unreached = object;
      object = object->next;
      if (previous != NULL) previous->next = object;
      else vm.objects = object;
      vm.gcFreed++;
      freeObject(unreached);
    }
  }
}

void collectGarbage() {
  size_t before = vm.bytesAllocated;
  if (flags.logGC) fprintf(stderr, "-- gc begin\n");

  markRoots();
  traceReferences();
  tableRemoveWhite(&vm.strings);  // weak table: forget strings that are about to die
  sweep();

  vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;
  vm.gcRuns++;

  if (flags.logGC) {
    fprintf(stderr, "-- gc end\n   collected %zu bytes (from %zu to %zu) next at %zu\n",
            before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
  }
}

void freeObjects() {
  Obj* object = vm.objects;
  while (object != NULL) {
    Obj* next = object->next;
    freeObject(object);
    object = next;
  }
  vm.objects = NULL;
  free(vm.grayStack);
  vm.grayStack = NULL;
  vm.grayCount = vm.grayCapacity = 0;
}
