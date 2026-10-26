#ifndef tp_vm_api_h
#define tp_vm_api_h

// The narrow C surface the web page talks to. Every function returns a
// pointer to a JSON string that stays valid until the next call.
//
//   tp_run(src)                  run a whole program
//   tp_begin(src, stress, limit) compile and get ready to single-step
//   tp_step()                    run exactly one bytecode instruction
//   tp_snapshot()                look at the machine without running it
//   tp_end()                     free everything

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define TP_API EMSCRIPTEN_KEEPALIVE
#else
#define TP_API
#endif

TP_API const char* tp_run(const char* source);
TP_API const char* tp_begin(const char* source, int stressGC, int stepLimit);
TP_API const char* tp_step(void);
TP_API const char* tp_snapshot(void);
TP_API void tp_end(void);

#endif
