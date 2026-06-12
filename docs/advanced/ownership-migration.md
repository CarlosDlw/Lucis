# Ownership Migration Guide

This guide covers breaking changes introduced by Lucis ownership/autodrop improvements.

## What Changed

- Heap-backed values (`string`, `vec`, `map`, `set`) are ownership-tracked.
- Reusing moved values now raises diagnostics.
- `freeStr` consumes ownership.
- Collection free paths now deep-drop string payloads.

## Before / After

### Use-after-move

Before (accepted, but unsafe):

```lucis
string a = fromCStrCopy(c"hello");
string b = a;
println(a);
```

After:

```lucis
string a = fromCStrCopy(c"hello");
string b = a;      // move
println(b);        // use destination
```

### Explicit free

Before:

```lucis
string s = fromCStrCopy(c"hello");
freeStr(s);
freeStr(s); // double free at runtime
```

After:

```lucis
string s = fromCStrCopy(c"hello");
freeStr(s); // consumed
// freeStr(s); // compile-time ownership error
```

## Recommended Migration Steps

1. Compile and fix `use-after-move` diagnostics first.
2. Replace accidental copies with explicit clone-style APIs when you need duplication.
3. Remove manual deep-free loops for `vec<string>`, `map<*, string>`, and `set<string>`.
4. Keep `defer` for non-Lucis resources (`fd`, raw pointers, mutexes).
