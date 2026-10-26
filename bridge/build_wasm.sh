#!/usr/bin/env bash
# Compile the C virtual machine to WebAssembly so the web page can run the
# REAL clox instead of only its JavaScript twin. Output: web/vm.js + web/vm.wasm.
#
# One-time setup (Fedora or anything else):
#   git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
#   cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
#   source ~/emsdk/emsdk_env.sh
# Then:  ./build_wasm.sh
set -euo pipefail
cd "$(dirname "$0")"

if ! command -v emcc >/dev/null 2>&1; then
  echo "emcc not found. Install the Emscripten SDK and run:  source ~/emsdk/emsdk_env.sh" >&2
  exit 1
fi

SRC=$(ls ../bytecode-vm/src/*.c | grep -v '/main\.c$')

emcc -O2 -std=c11 -I../bytecode-vm/src $SRC vm_api.c \
  -o ../web/vm.js \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createTracepaperVM \
  -s EXPORTED_FUNCTIONS='["_tp_run","_tp_begin","_tp_step","_tp_snapshot","_tp_end"]' \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
  -s STACK_SIZE=1048576 \
  -s ALLOW_MEMORY_GROWTH=1

echo "Built web/vm.js and web/vm.wasm"
echo "Serve the web folder (browsers will not load .wasm from file://):"
echo "  cd ../web && python3 -m http.server 8000"
