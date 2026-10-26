# Tracepaper

A browser tool that shows a beginner what a computer does with their first programs, built on the
interpreters from *Crafting Interpreters* by Robert Nystrom (MIT licence). It has nine short
chapters, each with its own picture and one thing to break. The garbage collector chapter draws the
heap as a grid of cells that fills up, gets marked, and gets swept.

## Try it in 10 seconds

Open `web/index.html` in a browser. It needs no build step and no internet (the page works offline;
only the font link is skipped).

## What is in the box

```
tracepaper/
├── web/            the teaching page (index.html)
├── bytecode-vm/    clox in C: scanner, compiler, VM, closures, mark-sweep garbage collector
├── tree-walk/      jlox in Java: scanner, parser, tree-walking interpreter (stops before classes)
├── bridge/         C API for the browser (tp_run / tp_step / tp_snapshot) + WebAssembly build script
├── tests/          shared Lox programs, run against all three implementations
├── docs/           synopsis.md, report/ (for the write-up)
└── test_all.sh     builds everything and runs every test
```

| Book chapters | Where |
|---|---|
| Part II ch. 4-10 (scanning, syntax trees, parsing, evaluating, statements, control flow, functions) | `tree-walk/com/tracepaper/lox/` |
| Part III ch. 14-25 (chunks, VM, scanner, compiler, values, strings, hash tables, variables, jumps, calls, closures) | `bytecode-vm/src/` |
| Part III ch. 26 (garbage collection) | `bytecode-vm/src/memory.c` |

Not included: classes, methods and inheritance (book ch. 12-13 and 27-29), and the Resolver (ch. 11).
`bytecode-vm/ROADMAP.md` lists exactly what is done.

## Run the pieces

```bash
# C virtual machine
cd bytecode-vm && make
./clox ../tests/programs/closures.lox
./clox --disasm  ../tests/programs/functions.lox     # show the bytecode
./clox --trace   x.lox                                # show the stack at every instruction
./clox --stress-gc --log-gc ../tests/programs/gc_strings.lox   # watch the collector (log goes to stderr)

# Java tree-walking interpreter (needs a JDK)
cd tree-walk && ./build.sh && ./jlox ../tests/programs/closures.lox

# everything, with tests
./test_all.sh
```

On Fedora: `sudo dnf install gcc make nodejs java-latest-openjdk-devel`.

## Run the real C VM inside the page (optional)

The page already contains a JavaScript engine that follows the same design, so it works out of the
box. If you also build the C VM to WebAssembly, the page runs every program through it as well and
shows "Checked against the real C virtual machine: same result" under the screen.

```bash
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest && source ./emsdk_env.sh
cd /path/to/tracepaper/bridge && ./build_wasm.sh        # writes web/vm.js and web/vm.wasm
cd ../web && python3 -m http.server 8000                # browsers will not load .wasm from file://
```

**Honest status:** the WebAssembly step was not run when this was generated (no Emscripten was
available). The C code behind it is tested natively (`make -C bridge test`), and the loader in
`index.html` fails silently, so a missing or broken `vm.wasm` just leaves the JavaScript engine in charge.

## What was tested

`./test_all.sh` runs:

* 15 Lox programs through clox, normally and with the collector forced to run on **every** allocation
* the same programs under AddressSanitizer and UBSan
* the same programs through jlox (the two compile errors that need the Resolver are tagged `clox-only`)
* the bridge API natively under AddressSanitizer
* the web page's JavaScript engine against the real C VM: identical outcome and output on all 15
* a headless run of the web page that opens every chapter, steps through every instruction, and
  checks that the Memory and Garbage collection tasks can be solved

Not tested: how the page looks and feels in a real browser. Open it and click through once.

## How the chapters map to lessons

1. **Adding two numbers**: the pile (stack) and the steps made from one line
2. **Variables and scope**: shelf (globals) and desk (locals)
3. **Decisions and loops**: the finger moving down the lines, and jumping back
4. **Functions and recursion**: windows stacking up (call frames)
5. **Closures**: backpacks (upvalues) that outlive their function
6. **Memory**: text lives on a heap of 8 cells; with no cleaner the program runs out
7. **Garbage collection**: the same program, now with mark and sweep; arrows from variables, cells dissolve
8. **Mistakes**: errors found before running vs while running
9. **Optimization**: the optimizer does constant work once, before the program runs

Every step shows one plain-English sentence. The raw instruction list, token list and step log are in
the collapsed "Extra: under the hood" panel, so nobody is buried in traces.

## Model simplifications (say these out loud in the demo)

* The page's garbage-collector animation models **text strings** only, on a fixed 8-cell grid. The real
  collector in `memory.c` handles strings, functions, closures and upvalues, and triggers by bytes
  allocated, not by cell count.
* Strings in the C VM are interned (equal text is one object). The page gives each `+` its own cell
  so the grid fills in an easy-to-follow way.
* `clock()` exists in the C VM and jlox but not in the web page.
* The page stops a program after 6000 steps. Without that, `while (true) {}` would freeze the tab.
* jlox has the scoping quirk the book fixes with the Resolver (ch. 11): a closure can see a variable
  declared after it in the same block. That is intentional here.

## Likely questions from the examiner

* **Why a stack?** Every operator takes its inputs from the top and leaves its result there, so the
  compiler needs no registers or temporary names. See `OP_ADD` in `vm.c`.
* **Why two interpreters?** jlox walks the syntax tree (easy to read). clox compiles to bytecode (fast,
  and what real languages do). Both run the same test programs and give the same answers.
* **What is a root?** Anything the running program can reach directly: the stack, the globals, the call
  frames, open upvalues, and functions still being compiled. See `markRoots` in `memory.c`.
* **How do you know the collector never frees something live?** The stress mode collects on every
  allocation, and the whole suite passes under AddressSanitizer in that mode.
* **What is mark-sweep?** Mark: starting from the roots, flag every object you can reach. Sweep: free
  every object that is not flagged. `collectGarbage` is about 15 lines.
* **What does each instruction mean?** `bytecode-vm/src/chunk.h` lists all 29; `./clox --disasm` shows
  them for any program, and the page's "Steps" tab explains each in plain English.
