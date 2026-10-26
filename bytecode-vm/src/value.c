#include <math.h>
#include "value.h"
#include "memory.h"
#include "object.h"

void initValueArray(ValueArray* array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void writeValueArray(ValueArray* array, Value value) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
  }
  array->values[array->count] = value;
  array->count++;
}

void freeValueArray(ValueArray* array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  initValueArray(array);
}

// Strings are interned (ch.20), so comparing object pointers is enough.
bool valuesEqual(Value a, Value b) {
  if (a.type != b.type) return false;
  switch (a.type) {
    case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL:    return true;
    case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_OBJ:    return AS_OBJ(a) == AS_OBJ(b);
    default:         return false;
  }
}

// The book prints numbers with "%g", which shows 3628800 as 3.6288e+06. That
// is baffling for a beginner, so whole numbers print in full and everything
// else gets 15 significant digits. (The web engine uses the same rule.)
static void printNumber(double n) {
  if (n == floor(n) && fabs(n) < 1e15) vmOut("%.0f", n == 0 ? 0.0 : n);
  else vmOut("%.15g", n);
}

void printValue(Value value) {
  switch (value.type) {
    case VAL_BOOL:   vmOut(AS_BOOL(value) ? "true" : "false"); break;
    case VAL_NIL:    vmOut("nil"); break;
    case VAL_NUMBER: printNumber(AS_NUMBER(value)); break;
    case VAL_OBJ:    printObject(value); break;
  }
}
