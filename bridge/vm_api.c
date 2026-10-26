#include <math.h>
#include <stdio.h>
#include <string.h>

#include "vm_api.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

// One machine at a time. `outBuf`/`errBuf` capture what the program prints and
// what the compiler / VM complain about; `json` holds the reply.
static Buf outBuf, errBuf, json;
static bool active = false;
static bool finished = false;
static bool failed = false;

// ---- tiny JSON writer ------------------------------------------------------

static void put(const char* s) { bufAppend(&json, s, strlen(s)); }
static void putf(const char* fmt, ...) {
  char tmp[128];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
  va_end(ap);
  if (n > 0) bufAppend(&json, tmp, (size_t)n < sizeof tmp ? (size_t)n : sizeof tmp - 1);
}

static void putStr(const char* s, size_t len) {
  put("\"");
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    switch (c) {
      case '"':  put("\\\""); break;
      case '\\': put("\\\\"); break;
      case '\n': put("\\n"); break;
      case '\r': put("\\r"); break;
      case '\t': put("\\t"); break;
      default:
        if (c < 0x20) putf("\\u%04x", c);
        else bufAppend(&json, (const char*)&c, 1);
    }
  }
  put("\"");
}

static void putNumber(double n) {
  char tmp[64];
  if (n == floor(n) && fabs(n) < 1e15) snprintf(tmp, sizeof tmp, "%.0f", n == 0 ? 0.0 : n);
  else snprintf(tmp, sizeof tmp, "%.15g", n);
  putStr(tmp, strlen(tmp));
}

static void putValue(Value v) {
  switch (v.type) {
    case VAL_NIL:    put("{\"t\":\"nil\"}"); break;
    case VAL_BOOL:   put(AS_BOOL(v) ? "{\"t\":\"bool\",\"v\":true}" : "{\"t\":\"bool\",\"v\":false}"); break;
    case VAL_NUMBER: put("{\"t\":\"number\",\"v\":"); putNumber(AS_NUMBER(v)); put("}"); break;
    case VAL_OBJ: {
      Obj* o = AS_OBJ(v);
      switch (o->type) {
        case OBJ_STRING:
          put("{\"t\":\"string\",\"id\":"); putf("%u", o->id); put(",\"v\":");
          putStr(((ObjString*)o)->chars, (size_t)((ObjString*)o)->length); put("}");
          break;
        case OBJ_CLOSURE:
        case OBJ_FUNCTION: {
          ObjFunction* f = o->type == OBJ_CLOSURE ? ((ObjClosure*)o)->function : (ObjFunction*)o;
          put("{\"t\":\"fn\",\"id\":"); putf("%u", o->id); put(",\"v\":");
          if (f->name) putStr(f->name->chars, (size_t)f->name->length); else putStr("<script>", 8);
          put("}");
          break;
        }
        default:
          put("{\"t\":\""); put(objTypeName(o->type)); putf("\",\"id\":%u}", o->id);
      }
    }
  }
}

static void putError(void) {
  if (errBuf.len) putStr(errBuf.data, errBuf.len); else put("null");
}

// ---- snapshot --------------------------------------------------------------

static void putSnapshot(void) {
  json.len = 0;
  put("{\"active\":"); put(active ? "true" : "false");
  put(",\"done\":"); put(finished ? "true" : "false");
  put(",\"failed\":"); put(failed ? "true" : "false");
  put(",\"error\":"); putError();
  putf(",\"errLine\":%d,\"steps\":%zu", vm.errLine, vm.steps);

  // The instruction that will run next.
  if (active && !finished && vm.frameCount > 0) {
    CallFrame* f = &vm.frames[vm.frameCount - 1];
    Chunk* c = &f->closure->function->chunk;
    int ip = (int)(f->ip - c->code);
    putf(",\"ip\":%d,\"line\":%d,\"op\":\"%s\"", ip, c->lines[ip], opcodeName(c->code[ip]));
  } else {
    put(",\"ip\":-1,\"line\":0,\"op\":null");
  }

  put(",\"stack\":[");
  for (Value* s = vm.stack; active && s < vm.stackTop; s++) {
    if (s != vm.stack) put(",");
    putValue(*s);
  }
  put("],\"globals\":[");
  bool first = true;
  for (int i = 0; active && i < vm.globals.capacity; i++) {
    Entry* e = &vm.globals.entries[i];
    if (e->key == NULL) continue;
    if (!first) put(",");
    first = false;
    put("{\"n\":"); putStr(e->key->chars, (size_t)e->key->length); put(",\"v\":"); putValue(e->value); put("}");
  }
  put("],\"frames\":[");
  for (int i = 0; active && i < vm.frameCount; i++) {
    CallFrame* f = &vm.frames[i];
    ObjFunction* fn = f->closure->function;
    int ip = (int)(f->ip - fn->chunk.code);
    if (i) put(",");
    put("{\"fn\":");
    if (fn->name) putStr(fn->name->chars, (size_t)fn->name->length); else putStr("<script>", 8);
    putf(",\"line\":%d}", fn->chunk.lines[ip > 0 ? ip - 1 : 0]);
  }
  put("],\"heap\":[");
  first = true;
  for (Obj* o = active ? vm.objects : NULL; o != NULL; o = o->next) {
    if (!first) put(",");
    first = false;
    putf("{\"id\":%u,\"t\":\"%s\",\"v\":", o->id, objTypeName(o->type));
    switch (o->type) {
      case OBJ_STRING:   putStr(((ObjString*)o)->chars, (size_t)((ObjString*)o)->length); break;
      case OBJ_FUNCTION: { ObjFunction* f = (ObjFunction*)o; if (f->name) putStr(f->name->chars, (size_t)f->name->length); else putStr("<script>", 8); break; }
      case OBJ_CLOSURE:  { ObjFunction* f = ((ObjClosure*)o)->function; if (f->name) putStr(f->name->chars, (size_t)f->name->length); else putStr("<script>", 8); break; }
      default:           putStr(objTypeName(o->type), strlen(objTypeName(o->type)));
    }
    put("}");
  }
  putf("],\"gc\":{\"runs\":%zu,\"freed\":%zu,\"bytes\":%zu,\"next\":%zu}", vm.gcRuns, vm.gcFreed, vm.bytesAllocated, vm.nextGC);
  put(",\"out\":"); putStr(outBuf.data ? outBuf.data : "", outBuf.len);
  put("}");
}

// ---- lifecycle -------------------------------------------------------------

TP_API void tp_end(void) {
  if (active) {
    freeVM();
    active = false;
  }
  vm.outBuf = vm.errBuf = NULL;
  bufFree(&outBuf);
  bufFree(&errBuf);
}

TP_API const char* tp_begin(const char* source, int stressGC, int stepLimit) {
  tp_end();
  flags.stressGC = stressGC != 0;
  initVM();
  vm.outBuf = &outBuf;
  vm.errBuf = &errBuf;
  vm.stepLimit = stepLimit > 0 ? (size_t)stepLimit : 0;
  active = true;
  finished = failed = false;
  if (!vmStart(source)) {
    finished = failed = true;  // compile error: nothing to run
  }
  putSnapshot();
  return json.data;
}

TP_API const char* tp_snapshot(void) {
  putSnapshot();
  return json.data;
}

TP_API const char* tp_step(void) {
  if (active && !finished) {
    StepResult r = vmStep();
    if (r == STEP_DONE) finished = true;
    if (r == STEP_ERROR) finished = failed = true;
  }
  putSnapshot();
  return json.data;
}

TP_API const char* tp_run(const char* source) {
  tp_begin(source, 0, 200000);
  while (active && !finished) {
    StepResult r = vmStep();
    if (r == STEP_DONE) finished = true;
    if (r == STEP_ERROR) finished = failed = true;
  }
  // Compact reply: output as a list of lines, not the full snapshot.
  json.len = 0;
  put("{\"ok\":"); put(failed ? "false" : "true");
  put(",\"out\":[");
  size_t start = 0;
  bool first = true;
  for (size_t i = 0; i < outBuf.len; i++) {
    if (outBuf.data[i] == '\n') {
      if (!first) put(",");
      first = false;
      putStr(outBuf.data + start, i - start);
      start = i + 1;
    }
  }
  put("],\"err\":"); putError();
  putf(",\"errLine\":%d,\"steps\":%zu,\"gc\":{\"runs\":%zu,\"freed\":%zu}}", vm.errLine, vm.steps, vm.gcRuns, vm.gcFreed);
  return json.data;
}
