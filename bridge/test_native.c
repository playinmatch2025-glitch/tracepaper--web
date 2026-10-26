// Exercises the bridge API without a browser: `make -C bridge test`.
#include <stdio.h>
#include <string.h>
#include "vm_api.h"

static int failures = 0;
static void expect(const char* what, const char* json, const char* needle) {
  if (strstr(json, needle)) { printf("ok    %s\n", what); return; }
  printf("FAIL  %s\n      wanted %s\n      got    %.300s\n", what, needle, json);
  failures++;
}

int main(void) {
  expect("run prints", tp_run("print 1 + 2; print \"a\" + \"b\";"), "\"out\":[\"3\",\"ab\"]");
  expect("run ok flag", tp_run("print 1;"), "\"ok\":true");
  expect("compile error", tp_run("print 1 +;"), "\"ok\":false");
  expect("runtime error", tp_run("print 1 + \"a\";"), "Operands must be two numbers or two strings.");
  expect("step limit", tp_run("while (true) {}"), "Step limit reached");
  // Lox has no string escapes, so a backslash is an ordinary character; JSON must escape it.
  expect("json escapes", tp_run("print \"a\\b\";"), "\"out\":[\"a\\\\b\"]");

  const char* s = tp_begin("var a = \"x\" + \"y\";\nprint a;", 0, 1000);
  expect("begin is ready", s, "\"done\":false");
  expect("first op", s, "\"op\":\"OP_CONSTANT\"");
  int guard = 0;
  while (!strstr(s, "\"done\":true") && guard++ < 100) s = tp_step();
  expect("stepped to the end", s, "\"out\":\"xy\\n\"");
  expect("global visible", s, "\"n\":\"a\"");
  expect("heap lists strings", s, "\"t\":\"string\"");

  // GC visible through the API: make garbage, run under stress, see frees.
  s = tp_begin("var s = \"\"; for (var i = 0; i < 50; i = i + 1) s = s + \"a\";", 1, 100000);
  while (!strstr(s, "\"done\":true") && guard++ < 100000) s = tp_step();
  expect("gc ran under stress", s, "\"gc\":{\"runs\":");
  expect("gc freed objects", s, "\"freed\":");
  tp_end();
  printf(failures ? "\n%d failure(s)\n" : "\nbridge OK\n", failures);
  return failures != 0;
}
