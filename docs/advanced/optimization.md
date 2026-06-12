# Optimization

The Lucis compiler uses LLVM's PassBuilder infrastructure to optimize generated IR before emitting machine code. Four optimization levels are available, from no optimization (default) to aggressive inlining and vectorization.

---

## Optimization Levels

| Level | Flag | Description |
|-------|------|-------------|
| O0 | *(default)* | No optimization — fastest compile, largest binary, best for debugging |
| O1 | `-o1` | Basic optimization — constant folding, dead code elimination, small inlines |
| O2 | `-o2` | Moderate optimization — vectorization, loop transforms, recommended for testing |
| O3 | `-o3` | Aggressive optimization — maximum performance, slowest compile |

### Usage

```bash
# No optimization (default)
lucis main.lc ./main

# O1 — good for development
lucis main.lc ./main -o1

# O2 — balanced, recommended for production-like testing
lucis main.lc ./main -o2

# O3 — maximum performance, use for release builds
lucis main.lc ./main -o3
```

---

## How It Works

The optimizer receives an LLVM module (the IR for one source file) and runs a pipeline of analysis and transformation passes.

### Pipeline Setup

1. **Create PassBuilder** — orchestrates all pass ordering and dependencies
2. **Create analysis managers** — cached results at four granularity levels:
   - `ModuleAnalysisManager` — module-wide analysis
   - `FunctionAnalysisManager` — per-function analysis
   - `CGSCCAnalysisManager` — call graph analysis (strongly connected components)
   - `LoopAnalysisManager` — per-loop analysis
3. **Register analyses** — make all LLVM analyses available to passes
4. **Cross-register proxies** — allow passes at one level to query analyses at another
5. **Build pipeline** — `buildPerModuleDefaultPipeline(level)` selects the right passes
6. **Run** — execute all passes on the module

### O0: No Optimization

Returns immediately — no passes run at all. The IR is emitted exactly as generated.

- Fastest compilation
- Largest binary size
- Slowest runtime
- Best for debugging (variable names preserved, no reordering)

### O1: Basic Optimization

LLVM's default pipeline with minimal transformations:

| Pass | Effect |
|------|--------|
| **SROA** | Replace aggregate stack variables with individual scalars |
| **EarlyCSE** | Eliminate common subexpressions |
| **ConstantPropagation** | Replace variables with known constant values |
| **DeadCodeElimination** | Remove unused assignments and branches |
| **DeadArgumentElimination** | Remove unused function parameters |
| **SimplifyAllocas** | Simplify stack allocation patterns |
| **InlinePass** | Inline small functions (< ~75 instructions) |
| **LoopRotate** | Rotate loops to simplify control flow |

**Characteristics:**
- ~2–3x longer compile time vs O0
- Significantly smaller binary
- ~10–30% faster runtime

### O2: Moderate Optimization

Everything from O1, plus more aggressive transforms:

| Pass | Effect |
|------|--------|
| **LoopVectorize** | Convert scalar loops to SIMD vector operations |
| **SLPVectorizer** | Find straight-line parallelism and vectorize |
| **VectorCombine** | Combine vector operations |
| **LoadStoreVectorizer** | Merge adjacent loads/stores into vector ops |
| **MemCpyOpt** | Optimize memory copy patterns |
| **PartialInlining** | Inline beneficial portions of functions |
| **AggressiveDCE** | More aggressive dead code removal |
| **GuardWidening** | Widen guard conditions for fewer checks |

**Characteristics:**
- ~4–6x longer compile time vs O0
- Most balanced option
- ~30–60% faster runtime

### O3: Aggressive Optimization

Everything from O2, plus maximum performance transforms:

| Pass | Effect |
|------|--------|
| **LoopUnroll** | Replicate loop bodies to reduce branch overhead |
| **LoopDistribute** | Split independent loops for better vectorization |
| **LoopFlatten** | Flatten nested loops |
| **JumpThreading** | Eliminate redundant branches |
| **SpeculativeExecution** | Execute code speculatively when profitable |
| **CallSiteSplitting** | Duplicate functions for specialized call sites |
| **InlinerPass** | Very aggressive inlining (< ~275 instructions) |
| **IPO** | Inter-procedural optimization across function boundaries |
| **MergeFunctions** | Detect and merge identical function bodies |

**Characteristics:**
- ~10–20x longer compile time vs O0
- Smallest binary size
- ~50–100% faster runtime (sometimes 2–3x)

---

## What the Passes Do

### Constant Folding

Computes constant expressions at compile time:

```
int32 x = 10 * 20 + 5;
```

```llvm
// Before optimization:
%0 = mul i32 10, 20
%x = add i32 %0, 5

// After (constant folded):
%x = i32 205
```

### Dead Code Elimination

Removes code that has no effect on the result:

```
int32 unused = 42;      // removed — never read
int32 result = 5 * 10;
ret result;
```

### Function Inlining

Replaces function calls with the function body:

```
fn add(int32 a, int32 b) int32 {
    ret a + b;
}

fn main() int32 {
    int32 sum = add(10, 20);   // replaced with: sum = 10 + 20
    ret 0;
}
```

Inlining thresholds:
- O1: ~75 instructions
- O2: ~225 instructions
- O3: ~275 instructions

### Loop Unrolling

Replicates loop body to reduce branch overhead (O3 only):

```
// Before:
for i = 0; i < 4; i += 1 {
    result += arr[i];
}

// After unroll:
result += arr[0];
result += arr[1];
result += arr[2];
result += arr[3];
```

### Vectorization

Converts scalar operations to SIMD vector operations (O2+):

```
// Scalar loop:
for i = 0; i < 100; i += 1 {
    c[i] = a[i] + b[i];
}

// Vectorized (processes 4 elements at once):
for i = 0; i < 100; i += 4 {
    // uses SSE/AVX vector add instruction
    c[i..i+4] = a[i..i+4] + b[i..i+4];
}
```

### Control Flow Simplification

Simplifies branching patterns:

```llvm
// Before:
br i1 %cond, label %true, label %false
true:
  br label %merge
false:
  br label %merge
merge:
  %result = phi i32 [1, %true], [0, %false]

// After:
%result = select i1 %cond, i32 1, i32 0
```

---

## Performance Impact

The actual speedup depends on your code. Compute-heavy loops benefit most from optimization. Simple programs with few loops see less improvement.

### Rough Guidelines

```
                    Compile Time    Runtime Speed    Binary Size
O0 (default)        1x              1x               largest
O1 (-o1)            2-3x            10-30% faster    smaller
O2 (-o2)            4-6x            30-60% faster    smaller
O3 (-o3)            10-20x          50-100% faster   smallest
```

### Recommendations

- **Development:** Use O1 for quick feedback with reasonable performance
- **Testing:** Use O2 for balanced compilation speed and runtime
- **Release:** Use O3 for maximum performance in final builds
- **Debugging:** Use default (O0) for accurate stack traces and variable inspection

---

## Integration in the Pipeline

Optimization happens **per source file**, after IR generation and before object emission:

```
For each source file:
    1. Generate LLVM IR (IRGen)
    2. Optimize IR (Optimizer, if -o1/o2/o3)
    3. Emit .o object file (CodeGen)
```

Each file produces its own LLVM module, which is optimized independently. There is no link-time optimization (LTO) — all optimization happens before linking.

---

## Debugging Optimized Code

Optimized code can be harder to debug because the compiler may:
- Inline functions (breaking stack traces)
- Reorder instructions
- Eliminate variables you're trying to inspect
- Unroll loops (making step-through confusing)

For debugging, either use O0 (default) or O1 at most. O2 and O3 make debugging significantly harder.

### Viewing Generated IR

To see the LLVM IR that the compiler produces (before and after optimization), compile without an output path:

```bash
# Prints IR to stdout (unoptimized)
lucis build main.lc --emit-llvm

# To see optimized IR, you'd need to add optimization flags
# Currently, IR stdout mode always shows unoptimized IR
```
