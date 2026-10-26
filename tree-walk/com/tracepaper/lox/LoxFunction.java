package com.tracepaper.lox;

import java.util.List;

// ch.10: a function value remembers the environment it was *declared* in
// (its closure), and each call gets a fresh scope hanging off that.
// Without the Resolver chapter (11) there is a known scoping bug: see README.
class LoxFunction implements LoxCallable {
  private final Stmt.Function declaration;
  private final Environment closure;

  LoxFunction(Stmt.Function declaration, Environment closure) {
    this.declaration = declaration;
    this.closure = closure;
  }

  @Override
  public int arity() { return declaration.params.size(); }

  @Override
  public Object call(Interpreter interpreter, List<Object> arguments) {
    Environment environment = new Environment(closure);
    for (int i = 0; i < declaration.params.size(); i++) {
      environment.define(declaration.params.get(i).lexeme, arguments.get(i));
    }
    try {
      interpreter.executeBlock(declaration.body, environment);
    } catch (Return returnValue) {
      return returnValue.value;
    }
    return null;
  }

  @Override
  public String toString() { return "<fn " + declaration.name.lexeme + ">"; }
}
