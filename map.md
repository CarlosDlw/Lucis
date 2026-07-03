# Array Bug & Improvement Map

## Prioridade

| # | Bug | Onde | Teste em `ztests/src/main.lc` |
|---|-----|------|-------------------------------|

---

## 🟥 P0 — Crash / Wrong Code

### 1. Struct field `[]T` perde tipo array no IR

**Onde:** provavelmente `src/IRBuilder/IRGen.cpp` (visita struct decl / init de struct)

**Problema:** `struct Dots { []int32 arr; }` gera `%Dots = type { i32 }` (só o elemento, sem array). Quando o init tenta `insertvalue %Dots zeroinitializer, [3 x i32] %arr, 0`, o LLVM rejeita porque `%Dots` espera `i32`, não `[3 x i32]`.

A struct field `[]int32` devia reter a dimensão de array (ou tornar-se `{ ptr, i64 }` para unsized). O checker parece resolver o tipo do field como `int32` (elemento puro) em vez de `[]int32`.

**Teste:**
```lucis
use stdio::printf;

struct Dots {
    []int32 arr;
}

fn main() int32 {
    []int32 arr = [1, 2, 3];
    [3]Dots dots = [Dots{ arr }, Dots{ arr }, Dots{ arr }];

    assertMsg(dots[0].arr[0] == 1, "Assert [0].[0] == 1");
    assertMsg(dots[1].arr[1] == 2, "Assert [1].[1] == 2");
    assertMsg(dots[2].arr[2] == 3, "Assert [2].[2] == 3");
    assertMsg(arr.len() == 3 as usize, "Assert arr.len() == 3");
    return 0;
}
```

---

### 2. `dots[0].arr[0]` — "invalid index base expression"

**Onde:** `src/checkers/Checker.cpp` (visita `indexExpr` + `fieldAccess` em cadeia)

**Problema:** O checker não reconhece `dots[0].arr[0]` — indexar array, pegar field, indexar de novo. O parse gera `indexExpr(fieldAccess(indexExpr(dots, 0), arr), 0)`, e o checker para no "invalid index base expression" porque o base type depois do field access não é reconhecido como array.

**Teste:** o mesmo que #1.

---

### 3. Substituição genérica perde `[]` em `[]T`

**Onde:** `src/checkers/Checker.cpp:9009-9019` — `resolveTypeSpecWithSubst`

**Problema:** Quando instancia `struct Vec<T> { []T data; }` com `Vec<int32>`, a substituição conta os `arrayDims` mas retorna apenas `elemType` (o tipo `T` interno), perdendo o `[]` externo. O campo `data` vira `int32` em vez de `[]int32`.

**Como consertar:** Guardar os `arrayDims` antes de chamar `resolveTypeSpecWithSubst` no elemento, depois reaplicá-los no tipo retornado.

**Teste:**
```lucis
use stdio::printf;

struct Vec<T> {
    []T data;
}

fn main() int32 {
    Vec<int32> v;
    v.data = [1, 2, 3];
    printf("len={d}\n", v.data.len());
    return 0;
}
```

---

### 4. Parâmetros `[]T` só funcionam com 5 de 31 métodos

**Onde:** `src/IRBuilder/IRGen.cpp:18984-19055`

**Problema:** Quando um array é passado como parâmetro `arr: []int32`, ele é rebaixado (lowered) para `{ptr, len}`. O dispatch de métodos array só trata `len`, `isEmpty`, `first`, `last`, `at`. Qualquer outro método (`sort`, `reverse`, `contains`, `sum`, `equals`, `copy`, `slice`, `fill`, `swap`, `toString`, `join`, `rotate`) cai no `else` que imprime `"lucis: expected array type for '...'"` e retorna `undef`.

**Como consertar:** Generalizar o código para operar sobre `{ptr, len}`. Para métodos que iteram, usar o ponteiro e tamanho diretamente.

**Teste:**
```lucis
use stdio::printf;

fn soma([]int32 arr) int32 {
    return arr.sum();
}

fn main() int32 {
    []int32 x = [10, 20, 30];
    printf("soma={d}\n", soma(x));
    return 0;
}
```

---

### 5. `join()` com elemento não-inteiro/float usa `%s` e crasha

**Onde:** `src/IRBuilder/IRGen.cpp:20013-20019`

**Problema:** Para tipos que não são int nem float, o código faz `fmtElem = "%s"` e passa o elemento como `void*`, assumindo que é um `char*`. Para structs ou outros tipos compostos, crasha.

**Como consertar:** Usar `toString()` do elemento em vez de `%s`, ou erro em compile time.

**Teste:**
```lucis
use stdio::printf;

fn main() int32 {
    [3]bool arr = [true, false, true];
    // arr.join(",")  — crash se bool não for int/float
    printf("ok\n");
    return 0;
}
```

---

### 6. `toString()` memory allocation sem free claro

**Onde:** `src/IRBuilder/IRGen.cpp:19829-19937`

**Problema:** `toString()` chama `lucis_allocString` que faz `malloc`. A string retornada é `{ptr, len}`. Não há free explícito.

**Como consertar:** Documentar ownership ou usar GC/arena.

**Teste:**
```lucis
use stdio::printf;

fn main() int32 {
    [3]int32 arr = [1, 2, 3];
    string s = arr.toString();
    printf("arr={s}\n", s);
    return 0;
}
```

---

## 🟧 P1 — Comportamento Incorreto

### 7. `.len` property retorna `int64`, método `.len()` retorna `usize`

**Onde:**
- `src/checkers/Checker.cpp:4247-4248` — `.len` field access → `int64`
- `src/types/MethodRegistry.cpp:532` — `.len()` method → `usize`

**Problema:** `arr.len` (field access) dá `int64`, `arr.len()` (método) dá `usize`.

**Como consertar:** Unificar para `usize`.

**Teste:**
```lucis
use stdio::printf;

fn main() int32 {
    [5]int32 arr = [1, 2, 3, 4, 5];
    int64 a = arr.len;     // field access
    int64 b = arr.len();   // method call
    printf("field={d} method={d}\n", a, b);
    return 0;
}
```

---

### 8. Array vazio `[]` não infere tipo

**Onde:** `src/checkers/Checker.cpp:5636-5650`

**Problema:** `[]int32 x = [];` dá erro porque o checker retorna `nullptr` para array vazio.

**Como consertar:** Usar o tipo alvo para inferir o tipo do literal vazio.

**Teste:**
```lucis
use stdio::printf;

fn main() int32 {
    []int32 x = [];
    printf("len={d}\n", x.len());
    return 0;
}
```

---

### 9. Array multidimensional auto-inferido sempre como `arrayDims=1`

**Onde:** `src/checkers/Checker.cpp:7560-7586` (~linha 7574)

**Problema:** `[[1,2],[3,4]]` assume `arrayDims = 1` em vez de 2.

**Como consertar:** Inferir recursivamente.

**Teste:**
```lucis
use stdio::printf;

fn main() int32 {
    mat = [[1, 2], [3, 4]];
    printf("elem={d}\n", mat[0][1]);
    return 0;
}
```

---

### 10. `sizeof` retorna `int64` em vez de `usize`

**Onde:** `src/checkers/Checker.cpp:3703-3719`

**Problema:** `sizeof([10]int32)` dá `int64`.

**Como consertar:** Mudar para `usize`.

**Teste:**
```lucis
use stdio::printf;

fn main() int32 {
    usize s = sizeof([10]int32);
    printf("sizeof={d}\n", s);
    return 0;
}
```

---

## 🟨 P2 — Segurança / Robustez

### 11. Sem bounds checking em runtime

**Onde:** `src/IRBuilder/IRGen.cpp:8348-8510`, `19139-19150`

**Problema:** `arr[100]` num array de tamanho 5 acessa fora dos limites silenciosamente.

**Como consertar:** Adicionar `icmp ult index, size` com branch para erro antes do GEP.

**Teste:**
```lucis
use stdio::printf;

fn main() int32 {
    [5]int32 arr = [0, 0, 0, 0, 0];
    // arr[10] = 42;  — antes: corrompe; depois: aborta
    printf("ok\n");
    return 0;
}
```

---

### 12. Opaque 4096-byte unsized array payload em enum

**Onde:** `src/types/TypeInfo.cpp:8, 19`

**Problema:** `kOpaqueUnsizedArrayPayloadBytes = 4096` — estourar esse limite corrompe silenciosamente.

**Como consertar:** Usar `{ptr, len}` em vez de blob fixo, ou emitir erro runtime.

**Teste:**
```lucis
use stdio::printf;

enum MaybeArr {
    None;
    Some([]int32);
}

fn main() int32 {
    MaybeArr x = MaybeArr::Some([1, 2, 3]);
    printf("ok\n");
    return 0;
}
```

---

### 13. `copy()` / `slice()` alocam na stack

**Onde:** `src/IRBuilder/IRGen.cpp:19410-19422`, `19423-19506`

**Problema:** `arr.copy()` faz `alloca` do tamanho do array. Para arrays grandes, stack overflow.

**Como consertar:** Usar `malloc` + `free` para arrays acima de threshold.

**Teste:**
```lucis
use stdio::printf;

fn main() int32 {
    [100_000]int32 big = [0; 100_000];
    // big.copy() antes: stack overflow; depois: ok
    printf("ok\n");
    return 0;
}
```

---

## 🟩 P3 — Performance

### 14. `sort()` usa Bubble Sort O(n²)

**Onde:** `src/IRBuilder/IRGen.cpp:19677-19766`

**Problema:** `arr.sort()` é bubble sort.

**Como consertar:** Substituir por Quicksort ou chamar `qsort` do C.

**Teste:**
```lucis
use stdio::printf;

fn main() int32 {
    [5]int32 arr = [5, 3, 1, 4, 2];
    arr.sort();
    printf("sorted={s}\n", arr.toString());
    return 0;
}
```

---

## ⏸️ P4 — Postergado (Melhoria)

### 15. Slice syntax `arr[2:5]`

**Onde:** grammar + checker + IRGen

### 16. `[SOME_CONST]int32` não suportado

**Onde:** grammar + checker

### 17. `==` / `!=` para arrays

**Onde:** checker + IRGen

---

## Resumo de Modificações

| Arquivo | O quê |
|---------|-------|
| `src/checkers/Checker.cpp:9009-9019` | #3 — preservar arrayDims na substituição genérica |
| `src/checkers/Checker.cpp:5636-5650` | #8 — array vazio inferir tipo |
| `src/checkers/Checker.cpp:7574-7575` | #9 — auto-inferir arrayDims > 1 |
| `src/checkers/Checker.cpp:3703-3719` | #10 — `sizeof` retornar `usize` |
| `src/checkers/Checker.cpp:4247-4248` | #7 — `.len` property retornar `usize` |
| `src/checkers/Checker.cpp` (index+field chain) | #2 — `dots[0].arr[0]` |
| `src/IRBuilder/IRGen.cpp` (struct init) | #1 — struct field `[]T` vira `{ i32 }` |
| `src/IRBuilder/IRGen.cpp:18984-19055` | #4 — generalizar dispatch para slice lowering |
| `src/IRBuilder/IRGen.cpp:20013-20019` | #5 — `join()` com tipos não numéricos |
| `src/IRBuilder/IRGen.cpp:19829-19937` | #6 — gerenciar memória de `toString()` |
| `src/IRBuilder/IRGen.cpp:8348-8510` | #11 — bounds checking |
| `src/IRBuilder/IRGen.cpp:19410-19506` | #13 — heap vs stack para arrays grandes |
| `src/IRBuilder/IRGen.cpp:19677-19766` | #14 — sort eficiente |
| `src/types/TypeInfo.cpp:8, 19` | #12 — opaque payload dinâmico |
