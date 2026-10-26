#!/usr/bin/env bash
# Runs every tests/programs/*.lox through a Lox implementation and compares:
#   // expect: TEXT            -> that line must appear, in order, on stdout
#   // expect-runtime-error    -> exit code 70
#   // expect-compile-error    -> exit code 65
# Usage: tests/run_tests.sh ./path/to/clox        (also runs with --stress-gc)
#        NO_STRESS=1 SKIP_TAG=clox-only tests/run_tests.sh ../tree-walk/jlox
set -u
BIN="${1:-../bytecode-vm/clox}"
DIR="$(cd "$(dirname "$0")" && pwd)/programs"
pass=0; fail=0
run_one() {
  local file="$1"; shift
  if [ -n "${SKIP_TAG:-}" ] && grep -q "$SKIP_TAG" "$file"; then return; fi
  local expected actual code want
  expected="$(grep -o '// expect: .*' "$file" | sed 's|// expect: ||')"
  actual="$("$BIN" "$@" "$file" 2>/dev/null)"; code=$?
  want=0
  grep -q 'expect-runtime-error' "$file" && want=70
  grep -q 'expect-compile-error' "$file" && want=65
  if [ "$code" -ne "$want" ] || { [ "$want" -eq 0 ] && [ "$actual" != "$expected" ]; }; then
    echo "FAIL  $(basename "$file") $*  (exit $code, wanted $want)"
    [ "$want" -eq 0 ] && diff <(echo "$expected") <(echo "$actual") | head -5
    fail=$((fail+1))
  else
    pass=$((pass+1))
  fi
}
for f in "$DIR"/*.lox; do
  run_one "$f"
  [ -n "${NO_STRESS:-}" ] || run_one "$f" --stress-gc   # jlox has no GC flags: set NO_STRESS=1
done
echo "passed $pass, failed $fail"
[ "$fail" -eq 0 ]
