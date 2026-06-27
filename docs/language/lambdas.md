# Lambdas (Closures)

This page covers lambda expressions, also known as anonymous functions or closures, in Lucis.

## Syntax

Lambdas are written using pipe-delimited parameter lists:

```
|int32 x| x * 2

|int32 a, int32 b| a + b

|int32 value, int32 subtrahend, int32 multiplier| {
    int32 result = value - subtrahend;
    ret result * multiplier;
}
```

There are two forms:

- **Expression lambda**: `|params| expression` — the body is a single expression, and its value is returned implicitly.
- **Block lambda**: `|params| { stmts }` — the body is a block with explicit `ret` statements.

## Parameters

Parameters follow the same `type name` syntax as regular functions:

```
|int32 x, float64 y| x + y
|string s| println(s)
|int32 ...rest| { /* variadic */ }
```

The variadic `...` syntax is also supported in lambda parameters.

## Type Inference

When a lambda is assigned to a variable with explicit type:

```
fn(int32) -> int32 f = |int32 x| x * 2;
```

Or used directly as an argument to a generic function:

```
fn doIt<T>(T target, fn(T)->T func) T {
    ret func(target);
}

fn main() int32 {
    auto x = doIt(10, |int32 x| x * 10);
    ret 0;
}
```

The return type is inferred from the lambda body. In expression lambdas it is the type of the body expression; in block lambdas it must match the declared return type of the enclosing context.

## Captures

Lambdas can capture variables from the enclosing scope:

```
fn main() int32 {
    int32 multiplier = 3;
    auto f = |int32 x| x * multiplier;
    printf("{d}\n", f(10));  // 30
    ret 0;
}
```

## Calling Lambdas

Lambdas are called using the same syntax as regular function calls:

```
auto f = |int32 x| x * 2;
f(5);  // 10
```

## LSP Support

The Lucis Language Server provides full support for lambdas:

- **Hover**: Hovering over a lambda variable shows the inferred signature: `fn(int32 x) -> int32`.
- **Completion**: Autocomplete suggests lambda parameter names and block-local variables inside the lambda body.
- **Signature Help**: Calling a lambda shows parameter hints: `f(int32 x) ?`.
- **Go to Definition**: Navigate to the lambda declaration and its parameters.
- **Semantic Tokens**: Lambda parameters are highlighted correctly.

