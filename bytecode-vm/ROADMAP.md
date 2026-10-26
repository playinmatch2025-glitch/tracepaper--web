# clox roadmap

Follows Part III of *Crafting Interpreters* (Robert Nystrom, MIT licence).

| Book chapter | What | Where | Status |
|---|---|---|---|
| 14 Chunks of Bytecode | opcodes, chunk, constants, disassembler | `chunk.*`, `debug.*` | done |
| 15 A Virtual Machine | stack machine, `vmStep` | `vm.c` | done |
| 16 Scanning on Demand | scanner | `scanner.*` | done (keyword table instead of the trie) |
| 17 Compiling Expressions | Pratt parser, single pass | `compiler.c` | done |
| 18 Types of Values | tagged `Value` | `value.*` | done |
| 19 Strings | `ObjString`, object list | `object.*` | done |
| 20 Hash Tables | open addressing, tombstones, interning | `table.*` | done |
| 21 Global Variables | `DEFINE/GET/SET_GLOBAL` | `compiler.c`, `vm.c` | done |
| 22 Local Variables | stack slots, scopes | `compiler.c` | done |
| 23 Jumping Back and Forth | `if`, `and`/`or`, `while`, `for` | `compiler.c` | done |
| 24 Calls and Functions | frames, natives (`clock`) | `vm.c`, `object.*` | done |
| 25 Closures | upvalues, open/closed | `compiler.c`, `vm.c` | done |
| 26 Garbage Collection | mark-sweep, gray stack, weak string pool, stress mode | `memory.c` | done |
| 27 Classes and Instances | | | not started |
| 28 Methods and Initializers | | | not started |
| 29 Superclasses | | | not started |
| 30 Optimization | hashing, NaN boxing | | not started |

Deliberate deviations from the book (all small):

* Runtime flags (`--trace`, `--disasm`, `--stress-gc`, `--log-gc`) instead of `#define DEBUG_*`.
* Output and error text go through `vmOut` / `vmErr` so the WASM bridge can capture them.
* `Obj` has an `id`, and the VM keeps counters, so a visualizer can name objects.
* `vmStep()` runs one instruction, so the bridge can single-step. `interpret()` just loops it.
* Numbers print in full when whole (`3628800`, not `3.6288e+06`) and with 15 digits otherwise.
* An optional step limit (`vm.stepLimit`) so a web page can survive `while (true) {}`.

Next step if there is time: chapters 27 to 29. The JS engine in `web/index.html` would need the
same additions, and `tests/engine_vs_clox.js` will tell you when the two disagree.
