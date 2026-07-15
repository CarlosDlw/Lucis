# Attributes

Attributes are metadata annotations placed before declarations using `#[...]` syntax:

```lucis
#[deprecated]
fn oldFunc() void {}

#[repr(C)]
struct Point { float64 x; float64 y; }

#[export]
#[no_mangle]
fn visible() void {}
```

Attributes with arguments use parentheses:

```lucis
#[link_section(".text.hot")]
fn hotPath() void {}

#[inline(always)]
fn tiny() void {}
```

---

## Attribute Reference

### `#[error]`
Marks an enum variant as the error variant. Required when the error variant name does not follow the naming convention (`Err`, `Error`, etc.).

```lucis
enum Result {
    #[error]
    Failure,
    Success,
}
```

**Applies to:** Enum variant  
**Arguments:** None  
**Checker effect:** Sets `isError = true` on the variant — used by `?` and `catch`  
**IR effect:** None (semantic marker only)

---

### `#[deprecated]`
Marks a declaration as deprecated. The checker emits a warning when a deprecated function is called.

```lucis
#[deprecated]
fn oldApi() void {}

#[deprecated("use newApi instead")]
fn legacy() void {}
```

**Applies to:** Any declaration  
**Arguments:** 0–2 string arguments (optional message and/or removal note)  
**Checker effect:** Emits warning on call  
**IR effect:** None

---

### `#[repr(…)]`
Controls the in-memory layout of structs and unions for ABI compatibility.

| Form | Description |
|------|-------------|
| `#[repr(C)]` | C-compatible layout — fields ordered as declared, no reordering |
| `#[repr(packed)]` | Packed layout — no padding between fields |
| `#[repr(transparent)]` | Same layout as the single non-zero-sized field |

```lucis
#[repr(C)]
struct Point { float64 x; float64 y; }

#[repr(packed)]
struct Header {
    uint8 flags;
    uint16 length;
}
```

**Applies to:** Struct, union, enum  
**Arguments:** 1+ ident arguments (`C`, `packed`, `transparent`)  
**Checker effect:** Validated  
**IR effect:** None currently (layout for `repr(C)` and `repr(packed)` is not yet implemented in IRGen)

---

### `#[no_mangle]`
Preserves the original function name in the object file (no module-path mangling).

```lucis
#[no_mangle]
fn startup() void {}
```

**Applies to:** Function, global const  
**Arguments:** None  
**IR effect:** Symbol name is kept as-is instead of being prefixed with the module path

---

### `#[export]`
Force external linkage for the symbol, making it visible to the linker.

```lucis
#[export]
fn api_entry() void {}
```

**Applies to:** Function, global const  
**Arguments:** None  
**IR effect:** Intended to set external linkage (currently a no-op — all functions already get `ExternalLinkage`)

---

### `#[link_section("…")]`
Places the function or global variable in a named ELF/MachO section.

```lucis
#[link_section(".text.hot")]
fn frequentlyCalled() void {}

#[link_section(".data.persistent")]
const CACHE_TABLE: int32 = 0;
```

**Applies to:** Function, global const  
**Arguments:** 1 string argument (the section name)  
**IR effect:** `llvm::GlobalObject::setSection()`

---

### `#[must_use]`
Warns if the return value of a function is discarded. Reserved for future use.

```lucis
#[must_use]
fn compute() int32 {}
```

**Applies to:** Function  
**Arguments:** None  
**Checker effect:** Validated but not yet enforced  
**IR effect:** None

---

### `#[noreturn]`
Declares that a function never returns (infinite loop, process exit, etc.).

```lucis
#[noreturn]
fn abort() void {
    loop { /* halt */ }
}
```

**Applies to:** Function  
**Arguments:** None  
**IR effect:** Sets LLVM `noreturn` attribute — enables dead-code elimination after calls

---

### `#[non_exhaustive]`
Declares that an enum may gain new variants in future versions of the library. Reserved for future use.

```lucis
#[non_exhaustive]
enum ErrorCode {
    NotFound,
    PermissionDenied,
}
```

**Applies to:** Enum  
**Arguments:** None  
**Checker effect:** Validated but not yet enforced  
**IR effect:** None

---

### `#[inline(…)]`
Controls function inlining behavior.

| Form | Description |
|------|-------------|
| `#[inline(always)]` | Force inlining — the function body is always inlined at every call site |
| `#[inline(never)]` | Prevent inlining — the function is never inlined |

```lucis
#[inline(always)]
fn fastPath(int32 x) int32 { x * 2 }

#[inline(never)]
fn coldPath() void { /* expensive setup */ }
```

**Applies to:** Function  
**Arguments:** 0–1 ident argument (`always` or `never`); bare `#[inline]` is valid but has no effect  
**IR effect:** Sets LLVM `alwaysinline` or `noinline` attribute

---

### `#[cold]`
Marks a function as rarely executed. LLVM uses this hint to optimize for size over speed and to block inlining.

```lucis
#[cold]
fn errorHandler() void { /* unrecoverable */ }
```

**Applies to:** Function  
**Arguments:** None  
**IR effect:** Sets LLVM `cold` attribute

---

### `#[hot]`
Marks a function as frequently executed. LLVM uses this hint to optimize more aggressively.

```lucis
#[hot]
fn renderFrame() void { /* performance-critical */ }
```

**Applies to:** Function  
**Arguments:** None  
**IR effect:** Sets LLVM `inlinehint` attribute (closest equivalent to "hot" in LLVM)

---

### `#[allow(…)]`
Suppresses specific lint warnings for the annotated declaration. Reserved for future use.

```lucis
#[allow(unused_variable)]
fn demo() void {
    int32 x = 42;
}
```

**Applies to:** Any declaration  
**Arguments:** 1+ arguments (lint warning names)  
**Checker effect:** Validated but not yet enforced  
**IR effect:** None

---

### `#[deny(…)]`
Denies (promotes to error) specific lint warnings for the annotated declaration. Reserved for future use.

**Applies to:** Any declaration  
**Arguments:** 1+ arguments (lint warning names)  
**Checker effect:** Validated but not yet enforced  
**IR effect:** None

---

### `#[doc("…")]`
Associates a documentation string with the declaration.

```lucis
#[doc("Computes the square root of x")]
fn sqrt(float64 x) float64 {}
```

**Applies to:** Any declaration  
**Arguments:** 1 string argument  
**Checker effect:** Validated  
**LSP effect:** Shown in hover tooltip  
**IR effect:** None

---

### `#[thread_local]`
Marks a global variable as thread-local storage. Each thread gets its own copy.

```lucis
#[thread_local]
const TLS_VAL: int32 = 42;
```

**Applies to:** Global const  
**Arguments:** None  
**IR effect:** `llvm::GlobalVariable::setThreadLocal(true)`

---

### `#[used]`
Prevents the linker from removing the symbol during dead-stripping.

```lucis
#[used]
const KEEP_ME: float64 = 3.14;
```

**Applies to:** Function, global const  
**Arguments:** None  
**IR effect:** Sets visibility to `Default` (intended but incomplete — does not yet set the `llvm.used` metadata)

---

### `#[optimize(…)]`
Provides optimization hints for a function.

| Form | Description |
|------|-------------|
| `#[optimize(speed)]` | Optimize the function for execution speed |
| `#[optimize(size)]` | Optimize the function for code size |

```lucis
#[optimize(speed)]
fn hotLoop() void {
    for int32 i in 0..1000000 { /* tight loop */ }
}

#[optimize(size)]
fn compact() void { /* minimal code */ }
```

**Applies to:** Function  
**Arguments:** 1 ident argument (`speed` or `size`)  
**IR effect:** Sets LLVM `OptimizeForSize` or string attribute `"optimize-for-speed"`

---

### `#[align(n)]`
Sets the minimum alignment of a global variable. The value must be a power of two.

```lucis
#[align(64)]
const CACHE_LINE: int32 = 0;
```

**Applies to:** Global const  
**Arguments:** 1 integer argument (power of two)  
**IR effect:** `llvm::GlobalVariable::setAlignment(Align(n))`

---

## Summary

| Attribute | Applies To | Arguments | IR Effect |
|-----------|-----------|-----------|-----------|
| `error` | Enum variant | None | Semantic marker only |
| `deprecated` | Any | 0–2 strings | None |
| `repr` | Struct, union, enum | 1+ idents | None |
| `no_mangle` | Function, const | None | Preserves symbol name |
| `export` | Function, const | None | No-op (planned) |
| `link_section` | Function, const | 1 string | `setSection()` |
| `must_use` | Function | None | None |
| `noreturn` | Function | None | `NoReturn` |
| `non_exhaustive` | Enum | None | None |
| `inline` | Function | 0–1 ident | `AlwaysInline`/`NoInline` |
| `cold` | Function | None | `Cold` |
| `hot` | Function | None | `InlineHint` |
| `allow` | Any | 1+ args | None |
| `deny` | Any | 1+ args | None |
| `doc` | Any | 1 string | None |
| `thread_local` | Const | None | `setThreadLocal()` |
| `used` | Function, const | None | `setVisibility()` |
| `optimize` | Function | 1 ident | `OptimizeForSize`/`optimize-for-speed` |
| `align` | Const | 1 int | `setAlignment()` |
