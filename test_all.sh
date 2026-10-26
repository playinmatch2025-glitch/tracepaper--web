#!/usr/bin/env bash
# Build everything and run every test. Needs: gcc, make, node, a JDK.
# Usage: ./test_all.sh
set -e
cd "$(dirname "$0")"
step() { printf '\n== %s ==\n' "$1"; }

step "clox (C): build + shared test programs, normal and with the collector running on every allocation"
make -C bytecode-vm
tests/run_tests.sh bytecode-vm/clox

step "clox under AddressSanitizer + UBSan, stress GC"
make -C bytecode-vm asan
tests/run_tests.sh bytecode-vm/clox-asan

step "jlox (Java): build + shared test programs"
tree-walk/build.sh
NO_STRESS=1 SKIP_TAG=clox-only tests/run_tests.sh tree-walk/jlox

step "bridge API (native, under ASan)"
make -C bridge test

step "web engine vs the real C VM"
node tests/engine_vs_clox.js

step "web page, headless smoke test"
node tests/web_smoke.js

printf '\nAll green.\n'
