# Tracepaper: Life of a line of code

**Aim.** A browser-based learning tool that lets an absolute beginner watch what a computer does
with their first programs, and change the program to see what happens. The learner should reach one
clear "aha" per chapter in about two minutes, and finish all nine chapters in about twenty.

**Approach.**

* One tiny language (Lox from *Crafting Interpreters*): numbers, text, variables, `if`, loops, functions, closures.
* Nine chapters, each with its own picture and one thing to break:
  adding, variables and scope, loops, functions and recursion, closures, memory, garbage collection,
  mistakes, optimization.
* Raw instruction traces are hidden by default. Each step is one plain-English sentence; the technical
  trace is under an optional "under the hood" panel.
* The garbage collector is a rectangular grid of heap cells: it fills up, the collector marks what
  variables can still reach, and the rest dissolves.

**What is built.**

| Part | What it is |
|---|---|
| `bytecode-vm/` | clox in C: scanner, single-pass compiler, bytecode VM, closures, mark-sweep garbage collector |
| `tree-walk/` | jlox in Java: scanner, parser, tree-walking interpreter (stops before classes) |
| `bridge/` | narrow C API (`tp_run`, `tp_step`, `tp_snapshot`) and the WebAssembly build script |
| `web/` | the teaching page |
| `tests/` | shared Lox programs run against clox, jlox and the web engine |

**Verification.** The same 15 programs give the same results in the C VM (also under AddressSanitizer
with the collector forced to run on every allocation), in the Java interpreter, and in the web engine.

**Out of scope (deliberately).** Classes and inheritance, optimization beyond four simple tricks,
a real-world standard library.
