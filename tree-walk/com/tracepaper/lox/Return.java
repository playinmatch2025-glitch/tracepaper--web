package com.tracepaper.lox;

// ch.10: `return` is implemented as an exception so it can unwind out of
// nested statements. Stack traces are switched off because this is control
// flow, not an error.
class Return extends RuntimeException {
  final Object value;

  Return(Object value) {
    super(null, null, false, false);
    this.value = value;
  }
}
