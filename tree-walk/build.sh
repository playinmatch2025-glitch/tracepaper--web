#!/usr/bin/env bash
# Generate Expr.java / Stmt.java, compile everything into ./out, and print
# how to run it. Needs a JDK (javac).  Usage: ./build.sh && ./jlox script.lox
set -euo pipefail
cd "$(dirname "$0")"
JAVAC=${JAVAC:-javac}
if ! command -v "$JAVAC" >/dev/null 2>&1; then
  # Some JREs ship the compiler module without the javac launcher.
  JAVAC="java -m jdk.compiler/com.sun.tools.javac.Main"
fi
mkdir -p out
$JAVAC -d out com/tracepaper/tool/GenerateAst.java
java -cp out com.tracepaper.tool.GenerateAst com/tracepaper/lox
$JAVAC -d out com/tracepaper/lox/*.java
echo "Built. Run:  ./jlox ../tests/programs/closures.lox"
