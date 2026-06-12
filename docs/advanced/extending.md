# Extending the Compiler

The Lucis compiler is designed to be extended in three ways: adding builtin functions, creating new stdlib modules, and introducing new language features. This page walks through each process with concrete examples.

---

## Adding a Builtin Function

A builtin function is a function that's available to Lucis programs through `use std::*` imports but is implemented in C. Adding one requires changes in three places.

### Example: `gcd(int32, int32) -> int32`

#### Step 1: Register in BuiltinRegistry

The `BuiltinRegistry` tells the checker what functions exist, their parameter types, and their return type.

**File:** `src/types/BuiltinRegistry.cpp`

```cpp
BuiltinRegistry::BuiltinRegistry() {
    // ... existing functions ...

    add("gcd", "int32", {"int32", "int32"});
}
```

The `add()` call registers:
- Function name: `"gcd"`
- Return type: `"int32"`
- Parameter types: `{"int32", "int32"}`

#### Step 2: Implement in C

Create the C implementation in the builtins directory.

**File:** `src/builtins/math/math.h`

```c
int32_t lucis_gcd(int32_t a, int32_t b);
```

**File:** `src/builtins/math/math.c`

```c
#include <stdint.h>

int32_t lucis_gcd(int32_t a, int32_t b) {
    while (b != 0) {
        int32_t temp = b;
        b = a % b;
        a = temp;
    }
    return a < 0 ? -a : a;
}
```

All builtin C functions are prefixed with `lucis_` to avoid name conflicts with system libraries.

#### Step 3: Add IR Generation

The IR generator needs to know how to emit a call to the C function.

**File:** `src/IRBuilder/IRGen.cpp` — inside `visitFnCallExpr()`

```cpp
if (funcName == "gcd") {
    auto* arg1 = std::any_cast<llvm::Value*>(visit(ctx->exprList()->expr(0)));
    auto* arg2 = std::any_cast<llvm::Value*>(visit(ctx->exprList()->expr(1)));

    auto* i32Ty = llvm::Type::getInt32Ty(*context_);
    auto callee = declareBuiltin("lucis_gcd", i32Ty, {i32Ty, i32Ty});

    return builder_->CreateCall(callee, {arg1, arg2});
}
```

#### Step 4: Use in Lucis Code

```
use std::math::gcd;

fn main() int32 {
    int32 result = gcd(48, 18);
    println(result);               // 6
    ret 0;
}
```

**What happens at each stage:**
1. Checker validates `gcd` exists, takes 2 `int32`, returns `int32`
2. IRGen emits `call i32 @lucis_gcd(i32 48, i32 18)`
3. Linker resolves `lucis_gcd` from the builtins static library (`liblucis.a`)

---

## Polymorphic Builtins

Some builtins work on multiple types. For example, `abs()` works on any numeric type. The compiler generates a different C function call depending on the argument type.

### Registration

```cpp
add("abs", "_numeric", {"_numeric"}, true);  // isPolymorphic=true
```

The special marker `_numeric` means "any numeric type." The `true` flag marks it as polymorphic.

### Type-Specific Implementations

Each concrete type gets its own C function:

```c
int32_t lucis_abs_i32(int32_t x) { return x < 0 ? -x : x; }
int64_t lucis_abs_i64(int64_t x) { return x < 0 ? -x : x; }
float   lucis_abs_f32(float x)   { return x < 0.0f ? -x : x; }
double  lucis_abs_f64(double x)  { return x < 0.0 ? -x : x; }
```

### IR Dispatch

The IR generator selects the right function based on the argument type's `builtinSuffix`:

```cpp
if (funcName == "abs") {
    auto* argVal = std::any_cast<llvm::Value*>(visit(ctx->exprList()->expr(0)));
    auto* argTypeInfo = resolveExprTypeInfo(ctx->exprList()->expr(0));

    std::string suffix = argTypeInfo->builtinSuffix;  // "i32", "i64", "f64"
    std::string absFuncName = "lucis_abs_" + suffix;

    auto* fnType = llvm::FunctionType::get(argVal->getType(), {argVal->getType()}, false);
    auto callee = declareBuiltin(absFuncName, fnType);

    return builder_->CreateCall(callee, {argVal});
}
```

### Usage

```
int32 a = abs(-42);             // calls lucis_abs_i32
int64 b = abs(-1000000i64);     // calls lucis_abs_i64
float64 c = abs(-3.14);         // calls lucis_abs_f64
```

---

## Variadic Builtins

Some builtins accept a variable number of arguments, like `sprintf`:

### Registration

```cpp
add("sprintf", "string", {"string"}, true, true);  // isPolymorphic=true, isVariadic=true
```

The second `true` marks the function as variadic — the checker allows extra arguments beyond the declared parameters.

### Checker Handling

```cpp
if (builtinSig->isVariadic) {
    if (numArgs < builtinSig->paramTypes.size()) {
        error(ctx, "not enough arguments");
        return;
    }
    // Extra arguments are allowed
} else {
    if (numArgs != builtinSig->paramTypes.size()) {
        error(ctx, "wrong number of arguments");
        return;
    }
}
```

---

## Adding a Stdlib Module

A stdlib module is a collection of related builtin functions grouped under a namespace. Adding a module is easier than adding individual builtins because the import system handles namespace resolution automatically.

### Example: Creating `std::random`

#### Step 1: Create Module Files

**File:** `src/builtins/random/random.h`

```c
#ifndef LUCIS_RANDOM_H
#define LUCIS_RANDOM_H

#include <stdint.h>

void    lucis_seed(uint32_t seed);
int32_t lucis_randInt(void);
int64_t lucis_randIntRange(int64_t min, int64_t max);
double  lucis_randFloat(void);

#endif
```

**File:** `src/builtins/random/random.c`

```c
#include "random.h"
#include <stdlib.h>

void lucis_seed(uint32_t seed) {
    srand(seed);
}

int32_t lucis_randInt(void) {
    return (int32_t)(rand() & 0x7FFFFFFF);
}

int64_t lucis_randIntRange(int64_t min, int64_t max) {
    if (min >= max) return min;
    return min + (rand() % (max - min));
}

double lucis_randFloat(void) {
    return ((double)rand()) / RAND_MAX;
}
```

#### Step 2: Register All Functions

**File:** `src/types/BuiltinRegistry.cpp`

```cpp
BuiltinRegistry::BuiltinRegistry() {
    // ... existing modules ...

    // std::random
    add("seed",         "void",    {"uint32"});
    add("randInt",      "int32",   {});
    add("randIntRange", "int64",   {"int64", "int64"});
    add("randFloat",    "float64", {});
}
```

The import resolver automatically picks up all functions registered in the `BuiltinRegistry`, so no extra import configuration is needed.

#### Step 3: Use in Lucis Code

```
use std::random::{seed, randIntRange};

fm main() int32 {
    seed(12345);
    int64 num = randIntRange(1, 100);
    println(num);
    ret 0;
}
```

---

## Adding an Extended Type

Extended types are generic collection types like `vec<T>`, `map<K,V>`, and `set<T>`. They have a special LLVM struct layout, type-specific methods, and automatic cleanup support.

### Example: Adding `Deque<T>` (Double-Ended Queue)

#### Step 1: Define the Layout

**File:** `src/types/ExtendedTypeRegistry.cpp`

```cpp
void ExtendedTypeRegistry::registerBuiltins() {
    // ... Vec, Map, Set, Task ...

    ExtendedTypeDescriptor deque;
    deque.baseName     = "Deque";
    deque.genericArity = 1;
    deque.layout       = {
        { "data",      "ptr"   },
        { "front_idx", "usize" },
        { "back_idx",  "usize" },
        { "cap",       "usize" },
    };
    deque.cPrefix = "lucis_deque";

    // Methods
    deque.methods.push_back({ "len",       {TypeKind::Extended}, {},       "usize" });
    deque.methods.push_back({ "isEmpty",   {TypeKind::Extended}, {},       "bool"  });
    deque.methods.push_back({ "pushFront", {TypeKind::Extended}, {"_elem"}, "void" });
    deque.methods.push_back({ "pushBack",  {TypeKind::Extended}, {"_elem"}, "void" });
    deque.methods.push_back({ "popFront",  {TypeKind::Extended}, {},       "_elem" });
    deque.methods.push_back({ "popBack",   {TypeKind::Extended}, {},       "_elem" });
    deque.methods.push_back({ "free",      {TypeKind::Extended}, {},       "void"  });

    registerType(std::move(deque));
}
```

The `_elem` marker means "same type as the generic parameter." This is resolved at call sites based on the concrete type (e.g., `Deque<int32>` makes `_elem` = `int32`).

#### Step 2: Implement the C Runtime

**File:** `src/builtins/collections/deque.h`

```c
#ifndef LUCIS_DEQUE_H
#define LUCIS_DEQUE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    void*  data;
    size_t front_idx;
    size_t back_idx;
    size_t cap;
} lucis_deque_header;

void   lucis_deque_init_i32(lucis_deque_header* d);
void   lucis_deque_free_i32(lucis_deque_header* d);
void   lucis_deque_pushBack_i32(lucis_deque_header* d, int32_t val);
void   lucis_deque_pushFront_i32(lucis_deque_header* d, int32_t val);
int32_t lucis_deque_popFront_i32(lucis_deque_header* d);
int32_t lucis_deque_popBack_i32(lucis_deque_header* d);
size_t lucis_deque_len_i32(const lucis_deque_header* d);
// ... repeat for i64, f64, str, etc.

#endif
```

Each concrete element type gets its own set of functions, following the `_suffix` pattern used by all collection types.

#### Step 3: No IR Changes Needed

The `ExtendedTypeRegistry` framework already handles method dispatch for any registered extended type. When the IR generator encounters a method call on a `Deque<int32>`, it:

1. Looks up `Deque` in the extended type registry
2. Finds the method descriptor for `pushBack`
3. Generates the suffixed function name: `lucis_deque_pushBack_i32`
4. Emits the call

#### Step 4: Usage

```
fn main() int32 {
    Deque<int32> d = [];
    d.pushBack(10);
    d.pushFront(5);
    d.pushBack(20);

    println(d.len());              // 3

    int32 front = d.popFront();    // 5
    int32 back = d.popBack();      // 20

    d.free();
    ret 0;
}
```

---

## Adding a Language Feature

Adding new syntax requires changes to the grammar, parser regeneration, type checking, and IR generation.

### Example: `while-else` Statement

A `while-else` executes the else block if the loop condition was false from the start (the loop body never executed).

#### Step 1: Update the Grammar

**File:** `grammar/LucisParser.g4`

```antlr
statement
    : varDeclStmt
    | assignStmt
    | whileStmt
    | whileElseStmt    // new rule
    | forInStmt
    | ...
    ;

whileElseStmt
    : 'while' '(' expr ')' block 'else' block
    ;
```

#### Step 2: Regenerate the Parser

```bash
antlr4 -Dlanguage=Cpp_runtime -visitor -no-listener \
    -o src/generated/ \
    grammar/LucisLexer.g4 grammar/LucisParser.g4
```

This regenerates the lexer, parser, and base visitor classes. The base visitor will have a new `visitWhileElseStmt()` method that returns a default value.

#### Step 3: Add Type Checking

**File:** `src/checkers/Checker.cpp`

```cpp
std::any Checker::visitWhileElseStmt(LucisParser::WhileElseStmtContext* ctx) {
    auto* condTypeInfo = resolveExprTypeInfo(ctx->expr());
    if (!condTypeInfo || condTypeInfo->kind != TypeKind::Bool) {
        error(ctx, "while condition must be bool");
        return {};
    }

    visit(ctx->block(0));  // while body
    visit(ctx->block(1));  // else body

    return {};
}
```

#### Step 4: Add IR Generation

**File:** `src/IRBuilder/IRGen.cpp`

```cpp
std::any IRGen::visitWhileElseStmt(LucisParser::WhileElseStmtContext* ctx) {
    auto* func = currentFunction_;
    auto* condBB = llvm::BasicBlock::Create(*context_, "while_cond", func);
    auto* bodyBB = llvm::BasicBlock::Create(*context_, "while_body", func);
    auto* elseBB = llvm::BasicBlock::Create(*context_, "while_else", func);
    auto* exitBB = llvm::BasicBlock::Create(*context_, "while_exit", func);

    builder_->CreateBr(condBB);

    // Condition check
    builder_->SetInsertPoint(condBB);
    auto* cond = std::any_cast<llvm::Value*>(visit(ctx->expr()));
    builder_->CreateCondBr(cond, bodyBB, elseBB);

    // Loop body
    builder_->SetInsertPoint(bodyBB);
    loopStack_.push_back({exitBB, condBB});
    visit(ctx->block(0));
    builder_->CreateBr(condBB);
    loopStack_.pop_back();

    // Else block (only runs if loop never executed)
    builder_->SetInsertPoint(elseBB);
    visit(ctx->block(1));
    builder_->CreateBr(exitBB);

    // Continue after while-else
    builder_->SetInsertPoint(exitBB);

    return {};
}
```

#### Step 5: Usage

```
int32 count = 0;

while (count < 0) {
    println("this never runs");
    count += 1;
} else {
    println("loop never executed!");
}
```

Output:
```
loop never executed!
```

---

## Adding Methods to Existing Types

You can add new methods to existing extended types like `vec<T>`.

### Example: `vec<T>.sum()` for Numeric Vecs

#### Step 1: Register the Method

**File:** `src/types/ExtendedTypeRegistry.cpp`

```cpp
// Inside Vec descriptor:
vec.methods.push_back({ "sum", {TypeKind::Extended}, {}, "_elem",
                        .requireNumeric = true });
```

The `.requireNumeric = true` constraint makes the checker reject `vec<string>.sum()` at compile time.

#### Step 2: Implement in C

**File:** `src/builtins/collections/vec.c`

```c
int32_t lucis_vec_sum_i32(const lucis_vec_header* v) {
    int32_t sum = 0;
    const int32_t* data = (const int32_t*)v->ptr;
    for (size_t i = 0; i < v->len; i++) {
        sum += data[i];
    }
    return sum;
}

// Repeat for i64, f32, f64...
```

#### Step 3: IR Generation

**File:** `src/IRBuilder/IRGen.cpp`

```cpp
if (methodName == "sum") {
    auto* recv = std::any_cast<llvm::Value*>(visit(ctx->expr()));
    auto* recvTypeInfo = resolveExprTypeInfo(ctx->expr());
    auto* elemTypeInfo = recvTypeInfo->elementType;

    std::string sumFn = "lucis_vec_sum_" + elemTypeInfo->builtinSuffix;
    auto* elemLLVMTy = elemTypeInfo->toLLVMType(*context_, dl);
    auto callee = declareBuiltin(sumFn, elemLLVMTy, {ptrTy});

    return builder_->CreateCall(callee, {recv});
}
```

#### Step 4: Usage

```
fn main() int32 {
    vec<int32> v = [1, 2, 3, 4, 5];
    int32 total = v.sum();
    println(total);                // 15
    v.free();
    ret 0;
}
```

---

## Design Principles

The compiler follows several principles that keep extensions consistent:

**C Runtime, T Wrapper:**
All core logic is implemented in C for portability and performance. Lucis provides nicer syntax on top. C functions are always prefixed with `lucis_`.

**Monomorphization:**
Generic types generate type-specific C functions. `vec<int32>` uses `lucis_vec_push_i32`, `vec<string>` uses `lucis_vec_push_str`. This trades binary size for type safety and performance (no runtime dispatch).

**Registry Pattern:**
All extensions register in central registries (`BuiltinRegistry`, `ExtendedTypeRegistry`, `TypeRegistry`). The checker and IR generator query these registries rather than hardcoding knowledge of specific types or functions. This makes adding new features non-invasive.

**Cleanup Semantics:**
Every collection type must register a `free` method. The auto-cleanup system scans locals at function exit and emits the appropriate `lucis_{type}_free_{suffix}()` call. Defer handles explicit ordering.

---

## Checklists

### Adding a Builtin Function
- [ ] Register in `BuiltinRegistry` (name, return type, param types)
- [ ] Implement C function in `src/builtins/` (header + source)
- [ ] Add IR generation in `IRGen::visitFnCallExpr()`
- [ ] Test with sample Lucis code

### Adding a Stdlib Module
- [ ] Create module directory `src/builtins/modulename/`
- [ ] Implement all functions in C (with `lucis_` prefix)
- [ ] Register all functions in `BuiltinRegistry`
- [ ] Test with `use std::modulename::*` imports

### Adding an Extended Type
- [ ] Register in `ExtendedTypeRegistry` (layout, methods, generic arity)
- [ ] Implement C runtime (type-specific functions for each suffix)
- [ ] Register `free` method for auto-cleanup
- [ ] Test monomorphization with multiple concrete types

### Adding a Language Feature
- [ ] Update grammar (`LucisParser.g4` and/or `LucisLexer.g4`)
- [ ] Regenerate parser with ANTLR4
- [ ] Add type checking in `Checker`
- [ ] Add IR generation in `IRGen`
- [ ] Handle cleanup/defer implications
- [ ] Test with complex examples
