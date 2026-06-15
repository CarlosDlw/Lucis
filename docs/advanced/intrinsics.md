# Creating Lucis Intrinsics

This guide explains how to extend the Lucis compiler with custom intrinsic functions, namespaces, and types. Intrinsics are built-in operations exposed as `lucis::namespace::function(...)` calls — always available without any `use` declaration.

## Architecture Overview

An intrinsic is registered in three layers:

| Layer | File | Purpose |
|-------|------|---------|
| **Definition** | `src/intrinsics/namespaces/*.cpp` | Declare the function, its params, return type, and lowering |
| **Checker** | `src/checkers/Checker.cpp` (auto) | Validates argument count and types at compile time |
| **IRGen** | `src/IRBuilder/IRGen.cpp` (auto) | Calls the lowering lambda to emit LLVM IR |

The LSP (completion, hover, signature help) picks up intrinsics automatically from the registry — no extra wiring needed.

---

## Step 1: Register a New Namespace

Each intrinsic namespace lives in its own `.cpp` file under `src/intrinsics/namespaces/`. The registration function receives an `IntrinsicRegistry&` and a `TypeRegistry&`.

### Minimal namespace (empty)

```cpp
// src/intrinsics/namespaces/MyNamespace.cpp
#include "intrinsics/IntrinsicRegistry.h"

void registerMyNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg) {
    IntrinsicNamespace ns;
    ns.name = "myns";
    ns.description =
        "My custom intrinsics.\n"
        "Always available without any `use` declaration.";

    // ... add functions here ...

    reg.registerNamespace(std::move(ns));
}
```

### Wire it into the registry constructor

```cpp
// src/intrinsics/IntrinsicRegistry.cpp
IntrinsicRegistry::IntrinsicRegistry(TypeRegistry& typeRegistry) {
    registerCoreNamespace(*this, typeRegistry);
    registerDebugNamespace(*this, typeRegistry);
    registerUnsafeNamespace(*this, typeRegistry);
    registerMyNamespace(*this, typeRegistry);     // ← add here
}
```

### Declare the registration function in the header

```cpp
// src/intrinsics/IntrinsicRegistry.h (near bottom)
void registerCoreNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg);
void registerDebugNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg);
void registerUnsafeNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg);
void registerMyNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg);  // ← add here
```

---

## Step 2: Add a Simple Intrinsic (No Arguments)

```cpp
// Inside registerMyNamespace, before reg.registerNamespace(...):

{
    IntrinsicFunction fn;
    fn.name = "hello";
    fn.returnType = "void";
    fn.description =
        "Prints a greeting.\n\n"
        "```lucis\n"
        "lucis::myns::hello();\n"
        "```";

    fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
    fn.lowering.emitIR = [](
        llvm::IRBuilder<>& builder,
        llvm::Module* module,
        llvm::LLVMContext& context,
        const TypeRegistry& typeRegistry,
        const std::vector<llvm::Value*>& args,
        const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

        // Access the C runtime's printf
        auto* i8PtrTy = llvm::PointerType::getUnqual(context);
        auto* i32Ty = llvm::Type::getInt32Ty(context);
        auto* msg = builder.CreateGlobalStringPtr("hello from intrinsic!\n");

        auto callee = module->getOrInsertFunction(
            "printf", i32Ty, i8PtrTy);
        builder.CreateCall(callee, {msg});

        // Return undef for void functions
        return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
    };

    ns.functions.push_back(std::move(fn));
}
```

**Usage in Lucis:**
```lucis
lucis::myns::hello();
```

---

## Step 3: Add an Intrinsic with Fixed Parameters

Use `fn.params` to declare the required arguments. The checker validates both count and types.

```cpp
{
    IntrinsicFunction fn;
    fn.name = "double_it";
    fn.returnType = "int32";
    fn.params.push_back({"int32", false});  // type name, isVariadic (unused)
    fn.description =
        "Returns the input value multiplied by 2.\n\n"
        "```lucis\n"
        "int32 result = lucis::myns::double_it(21);  // 42\n"
        "```";

    fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
    fn.lowering.emitIR = [](
        llvm::IRBuilder<>& builder,
        llvm::Module* module,
        llvm::LLVMContext& context,
        const TypeRegistry& typeRegistry,
        const std::vector<llvm::Value*>& args,
        const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

        // args[0] is the first (and only) argument
        auto* i32Ty = llvm::Type::getInt32Ty(context);
        auto* two = llvm::ConstantInt::get(i32Ty, 2);
        return builder.CreateMul(args[0], two, "doubled");
    };

    ns.functions.push_back(std::move(fn));
}
```

**Usage:**
```lucis
int32 x = lucis::myns::double_it(21);  // 42
```

---

## Step 4: Add a Variadic Intrinsic (Untyped `...`)

Set `isVariadic = true` and add any fixed params before it. The checker allows `>= fixedCount` arguments and validates types only for the fixed ones.

```cpp
{
    IntrinsicFunction fn;
    fn.name = "sum";
    fn.returnType = "int32";
    fn.isVariadic = true;
    fn.description =
        "Sums all variadic arguments.\n\n"
        "```lucis\n"
        "int32 total = lucis::core::sum(1, 2, 3);  // 6\n"
        "```";

    fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
    fn.lowering.emitIR = [](
        llvm::IRBuilder<>& builder,
        llvm::Module* module,
        llvm::LLVMContext& context,
        const TypeRegistry& typeRegistry,
        const std::vector<llvm::Value*>& args,
        const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

        auto* i32Ty = llvm::Type::getInt32Ty(context);
        llvm::Value* sum = llvm::ConstantInt::get(i32Ty, 0);
        for (auto* arg : args)
            sum = builder.CreateAdd(sum, arg, "sum");
        return sum;
    };

    ns.functions.push_back(std::move(fn));
}
```

### With a Fixed Parameter Before `...`

```cpp
{
    IntrinsicFunction fn;
    fn.name = "sum_prefix";
    fn.returnType = "int32";
    fn.isVariadic = true;
    fn.params.push_back({"int32", false});  // required first param: int32 prefix
    fn.description =
        "Adds a prefix to the sum of variadic arguments.\n\n"
        "```lucis\n"
        "int32 total = lucis::myns::sum_prefix(100, 1, 2, 3);  // 106\n"
        "```";

    fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
    fn.lowering.emitIR = [](
        llvm::IRBuilder<>& builder,
        llvm::Module* module,
        llvm::LLVMContext& context,
        const TypeRegistry& typeRegistry,
        const std::vector<llvm::Value*>& args,
        const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

        auto* i32Ty = llvm::Type::getInt32Ty(context);
        llvm::Value* sum = args[0];  // first arg is the fixed prefix
        for (size_t i = 1; i < args.size(); i++)
            sum = builder.CreateAdd(sum, args[i], "sum");
        return sum;
    };

    ns.functions.push_back(std::move(fn));
}
```

---

## Step 5: Call an LLVM Intrinsic Directly

Use `Lowering::LLVMIntrinsic` and set `intrinsicName` to the LLVM intrinsic name.

```cpp
// Example: lucis::myns::trap() → @llvm.trap
{
    IntrinsicFunction fn;
    fn.name = "trap";
    fn.returnType = "void";
    fn.description = "Aborts execution via hardware trap.";

    fn.lowering.kind = IntrinsicFunction::Lowering::LLVMIntrinsic;
    fn.lowering.intrinsicName = "llvm.trap";

    ns.functions.push_back(std::move(fn));
}
```

The IRGen emits a call to the named LLVM intrinsic and inserts an `unreachable` instruction after it. Note that `LLVMIntrinsic` lowering does **not** pass arguments — use `InlineIR` if you need to forward arguments.

---

## Step 6: Call a C Runtime Function

Use `Lowering::BuiltinCall`. The IRGen calls `lucis_<intrinsicName>` with all arguments.

```cpp
{
    IntrinsicFunction fn;
    fn.name = "abort";
    fn.returnType = "void";
    fn.description = "Calls abort() from the C runtime.";

    fn.lowering.kind = IntrinsicFunction::Lowering::BuiltinCall;
    fn.lowering.intrinsicName = "abort";  // calls lucis_abort

    ns.functions.push_back(std::move(fn));
}
```

The C function must be linked into the final binary (e.g., in `src/builtins/`).

---

## Step 7: Register Custom Types

Intrinsic namespaces can register new types at setup time via the `TypeRegistry& typeReg` parameter. The types become globally available in the language and the LSP.

```cpp
void registerMyNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg) {

    // Register a simple handle type
    {
        TypeInfo handle;
        handle.name = "my_handle";
        handle.kind = TypeKind::Integer;
        handle.bitWidth = 64;
        handle.isSigned = false;
        handle.builtinSuffix = "u64";
        typeReg.registerType(std::move(handle));
    }

    // Register a pointer type pointing to an existing type
    {
        TypeInfo ptr;
        ptr.name = "my_ref";
        ptr.kind = TypeKind::Pointer;
        ptr.pointeeType = typeReg.lookup("my_handle");
        ptr.builtinSuffix = "ptr";
        typeReg.registerType(std::move(ptr));
    }

    // Use the custom type in intrinsic parameters
    {
        IntrinsicFunction fn;
        fn.name = "create_handle";
        fn.returnType = "my_handle";
        fn.description = "Creates a new handle.";
        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 42);
        };
        ns.functions.push_back(std::move(fn));
    }

    // ... register namespace ...
}
```

**LSP visibility:** Types registered via `typeReg.registerType(...)` are automatically discovered by `TypeRegistry::allTypes()` and surfaced in LSP completions.

---

## Step 8: Create a Generic Intrinsic

Set `isGeneric = true`. The function receives `typeArgs` with the resolved type arguments from the call site.

```cpp
{
    IntrinsicFunction fn;
    fn.name = "to_bits";
    fn.isGeneric = true;
    fn.returnType = "_any";
    fn.params.push_back({"_any", false});
    fn.description =
        "Reinterprets the bits of a value as an integer.\n\n"
        "```lucis\n"
        "int64 bits = lucis::myns::to_bits<int64>(3.14);\n"
        "```";

    fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
    fn.lowering.emitIR = [](
        llvm::IRBuilder<>& builder,
        llvm::Module* module,
        llvm::LLVMContext& context,
        const TypeRegistry& typeRegistry,
        const std::vector<llvm::Value*>& args,
        const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

        // typeArgs[0] is the resolved return type from <int64>
        auto& dl = module->getDataLayout();
        auto* targetTy = typeArgs[0]->toLLVMType(context, dl);
        return builder.CreateBitCast(args[0], targetTy, "bits");
    };

    ns.functions.push_back(std::move(fn));
}
```

Generic intrinsics require special handling in the checker. See `Checker.cpp` lines ~4105 for `va_arg<T>` as a reference.

---

## Lowering Reference

| Field | Value | Behaviour |
|-------|-------|-----------|
| `Lowering::LLVMIntrinsic` | `intrinsicName = "llvm.xxx"` | Declares and calls the LLVM intrinsic, then emits `unreachable`. No arguments forwarded. |
| `Lowering::BuiltinCall` | `intrinsicName = "func"` | Calls `lucis_func` from the C runtime with all arguments. |
| `Lowering::InlineIR` | `emitIR` lambda | Emits arbitrary LLVM IR via the builder. Receives all arguments and type args. Most flexible. |

---

## Struct Reference

### `IntrinsicFunction`

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `name` | `string` | — | Function name used in Lucis code (`lucis::ns::name`) |
| `returnType` | `string` | — | Lucis type name of the return value |
| `params` | `vector<IntrinsicParam>` | empty | Fixed parameter declarations |
| `isVariadic` | `bool` | `false` | Accepts unlimited extra untyped arguments |
| `isGeneric` | `bool` | `false` | Accepts type parameters (`name<T>`) |
| `returnsOwned` | `bool` | `false` | Return value transfers ownership |
| `consumingArgs` | `vector<size_t>` | empty | Indices of arguments that consume ownership |
| `borrowedArgs` | `vector<size_t>` | empty | Indices of arguments that borrow |
| `description` | `string` | — | Markdown description for LSP hover/docs |
| `lowering` | `Lowering` | `LLVMIntrinsic` | How IR is generated |

### `IntrinsicParam`

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `type` | `string` | — | Lucis type name (`"int32"`, `"string"`, `"_any"`) |
| `isVariadic` | `bool` | `false` | Reserved for future use |

### `Lowering::emitIR` Lambda Signature

```cpp
[](llvm::IRBuilder<>& builder,
   llvm::Module* module,
   llvm::LLVMContext& context,
   const TypeRegistry& typeRegistry,
   const std::vector<llvm::Value*>& args,      // evaluated argument values
   const std::vector<const TypeInfo*>& typeArgs // resolved type params (generics)
) -> llvm::Value*
```

---

## Checking What You've Built

After adding your intrinsic and rebuilding the compiler:

```bash
cmake --build build
```

Test it with a simple Lucis file:

```lucis
use std::log::println;

fn main() int32 {
    lucis::myns::hello();
    int32 x = lucis::myns::double_it(21);
    println(x);
    int32 s = lucis::myns::sum(10, 20, 30);
    println(s);
    ret 0;
}
```

Check error handling:

```bash
# Wrong type
lucis::myns::double_it("bad");
# → intrinsic 'myns::double_it' argument 1: expected 'int32', got 'string'

# Missing args
lucis::myns::double_it();
# → intrinsic 'myns::double_it' expects 1 argument(s), got 0

# Unknown intrinsic
lucis::myns::nonexistent();
# → unknown intrinsic 'myns::nonexistent'
```
