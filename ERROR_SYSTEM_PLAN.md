# Plano de Melhorias: Sistema de Erros da Linguagem Lucis

## Diagnóstico Atual

O Lucis tem dois mecanismos de erro complementares (`try/catch/throw` estilo exceções + `Result<T>` estilo Rust), ambos funcionais e com boa documentação. Porém, há 8 lacunas que limitam robustez em código real.

---

## Fase 1: `defer` no `throw` — Corrigir vazamento de recursos

### Problema
`throw` usa `longjmp` que salta sobre os cleanups. `defer` e auto-cleanups declarados antes de um `throw` nunca executam.

### Implementação
- **Arquivo**: `src/IRBuilder/IRGen.cpp` — `visitThrowStmt()` (~linha 18508)
- **Mudança**: Antes do `CreateCall(lucis_eh_throw)`, chamar `emitAllCleanups()` para disparar defers e auto-drops.
- **Atenção**: `emitAllCleanups()` modifica `locals_` e `deferStack_`. O `longjmp` não retorna, então estado corrompido não importa.
- **Arquivo**: `src/builtins/error/error.c` — opcional: garantir que `lucis_eh_throw` não depende de estado da stack após longjmp.

### Testes em `ztests/`
```lucis
// ztests/src/error_defer_throw.lc
// Teste 1: defer antes de throw — o defer deve executar
fn test_defer_before_throw() {
    auto cleaned = false;
    defer cleaned = true;  // deve executar mesmo com throw abaixo
    throw Error { message: "fail" };
}
// Garantia: cleaned == true após o catch

// Teste 2: múltiplos defers em ordem LIFO
fn test_defer_order_on_throw() {
    auto order = "";
    defer order = order.concat("C");
    defer order = order.concat("B");
    defer order = order.concat("A");
    throw Error { message: "fail" };
}
// Garantia: order == "ABC" após o catch

// Teste 3: auto-cleanup de resource antes do throw
fn test_auto_drop_before_throw() {
    auto v = vec<int32>();
    v.push(42);
    throw Error { message: "fail" };
    // v nunca é usado após o throw
}
// Garantia: sem vazamento de memória (rodar com ASAN/LSAN)
```

### Garantias
- [ ] `defer` dispara antes do `throw` (ordem LIFO)
- [ ] Auto-cleanup de recursos owned dispara antes do `throw`
- [ ] `longjmp` não corrompe estado (testar com múltiplos throws aninhados)
- [ ] Funções sem `try`/`catch` (unhandled throw) também executam defers
- [ ] `make test` (66 testes) continua passando

---

## Fase 2: Relaxar restrição de tipo no `?` — Aceitar enums compatíveis

### Problema
`currentReturnType_ == sourceType` é estrito demais. Se a função retorna `CResult<int32, string>` e a chamada produz `Result<int32>`, o `?` deveria funcionar se ambos têm a mesma convenção de erro.

### Implementação
- **Arquivo**: `src/checkers/Checker.cpp` — `visitPropagateExpr()` (~linha 2201) e `classifyUnwrapCatchEnum()` (~linha 28)
- **Mudança**: Em vez de `==`, verificar se:
  1. Ambos são enums com 2 variantes
  2. Ambos têm o mesmo tipo de payload de erro (nome e estrutura)
  3. O payload de sucesso é assignable ( `isAssignable(successPayload, currentReturnSuccess)` )
- Se compatível, emitir código de conversão automática no IRGen (wrap/unwrap dos payloads).
- **Arquivo**: `src/IRBuilder/IRGen.cpp` — `visitPropagateExpr()` (~linha 18732)
- **Mudança**: Se os tipos são diferentes mas compatíveis, gerar conversão: extrair payload de sucesso/erro do source e construir variante equivalente no tipo de destino.

### Testes em `ztests/`
```lucis
// ztests/src/error_compatible_enums.lc
enum MyResult<T> { Ok(T), Err(Error) }
enum YourResult<T, E> { Success(T), Failure(E) }

type DivResult = MyResult<int32>;

fn compute() DivResult { ... }
fn process() YourResult<int32, Error> {
    auto val = compute() ?;  // Deve compilar: MyResult<int32> → YourResult<int32, Error>
    // Ambos têm variante de erro com Error, sucesso com int32
    ret YourResult::Success(val);
}

// Teste de propagação cross-file
// ztests/src/lib/ops.lc
enum OpResult<T> { Value(T), Fault(Error) }
fn divide_op(int32 a, int32 b) OpResult<int32> { ... }

// ztests/src/main.lc
use lib::ops::{divide_op, OpResult};
fn calculate() MyResult<int32> {
    auto v = divide_op(10, 2) ?;  // Compatível: ambos erro=Error, sucesso=int32
    ret MyResult::Ok(v);
}
```

### Garantias
- [ ] `?` funciona entre enums com mesma estrutura de erro
- [ ] Conversão automática não quebra tipos (verificar com `catch`)
- [ ] Erro claro quando enums são INCOMPATÍVEIS (ex: payload de erro diferente)
- [ ] `make test` (66 testes) continua passando
- [ ] Cross-file compatible enums funcionam

---

## Fase 3: `?` em funções `void`

### Problema
`f()?;` não compila em função `void`. Deveria propagar o erro e descartar o sucesso.

### Implementação
- **Arquivo**: `src/checkers/Checker.cpp` — `visitPropagateExpr()`
- **Mudança**: Se `currentReturnType_->kind == TypeKind::Void`, permitir `?` desde que a expressão fonte tenha um enum com variante de erro compatível. O valor de sucesso é descartado.
- **Arquivo**: `src/IRBuilder/IRGen.cpp` — `visitPropagateExpr()`
- **Mudança**: No branch de erro, early-return com valor void. No branch de sucesso, descartar payload e continuar.

### Testes em `ztests/`
```lucis
// ztests/src/error_void_propagate.lc
enum MaybeError { Ok, Err(Error) }

fn fallible() MaybeError { ... }

fn main() void {
    fallible() ?;  // Deve compilar: propaga erro, descarta Ok
    println("success");
}
```

### Garantias
- [ ] `?` funciona em funções `void`
- [ ] Payload de sucesso é descartado sem efeitos colaterais (move/drop)
- [ ] Erro é propagado corretamente com `ret`
- [ ] `make test` continua passando

---

## Fase 4: Atributo `#[error]` para variantes customizadas

### Problema
Só `Err`, `Error`, `Failure`, `Fail`, `None` são reconhecidos como variante de erro. Nomes customizados (ex: `Problem`, `Fault`) são rejeitados.

### Implementação
- **Arquivo**: `grammar/LucisParser.g4` — adicionar regra de atributo
- **Arquivo**: `grammar/LucisLexer.g4` — token `ATTR_ERROR` ou reuso de `IDENTIFIER`
- **Sintaxe proposta**:
  ```lucis
  enum MyResult<T> {
      #[error]
      Fault(Error),
      Good(T)
  }
  ```
- **Arquivo**: `src/checkers/Checker.cpp` — `classifyUnwrapCatchEnum()`
- **Mudança**: Se nenhuma variante tem nome convencional, procurar por atributo `#[error]`.
- **Arquivo**: `src/IRBuilder/IRGen.cpp` — `classifyUnwrapCatchEnum()`
- **Mudança**: Mesma lógica, versão IRGen.

### Testes em `ztests/`
```lucis
// ztests/src/error_custom_variant.lc
enum Status<T> {
    #[error]
    Fault(Error),
    Good(T)
}

fn try_something() Status<int32> {
    ret Status::Fault(Error { message: "broken" });
}

fn test_custom_variant_catch() {
    auto val = try_something() catch {
        println("got: {s}", it.message);  // 'it' deve ser Error
        ret 0;
    };
    println("value: {d}", val);
}

fn test_custom_variant_propagate() Status<int32> {
    auto v = try_something() ?;  // Deve propagar Fault(Error)
    ret Status::Good(v);
}
```

### Garantias
- [ ] `#[error]` é reconhecido pelo checker e IRGen
- [ ] `?` e `catch` funcionam com variante customizada
- [ ] Compatível com enums existentes (fallback para nomes convencionais)
- [ ] Erro se mais de uma variante tem `#[error]`
- [ ] Erro se nenhuma variante é marcada e nenhum nome convencional
- [ ] `make test` continua passando

---

## Fase 5: Wrapping/contexto em erros propagados

### Problema
Não há como adicionar contexto ao propagar erro com `?`. Informação de onde o erro ocorreu na cadeia se perde.

### Implementação
- **Sintaxe proposta**:
  ```lucis
  auto v = load_config() ? { "failed to load config" };
  auto v = load_config() ? { "config for user '{}'", username };
  ```
  O `?` seguido de `{ ... }` captura o erro, adiciona contexto, e re-propaga.
- **Arquivo**: `grammar/LucisParser.g4` — regra `propagateExpr` com bloco opcional
- **Arquivo**: `src/checkers/Checker.cpp` — type-check do contexto
- **Arquivo**: `src/IRBuilder/IRGen.cpp` — extrair erro original, concatenar mensagem, re-propagate

### Testes em `ztests/`
```lucis
// ztests/src/error_context_wrap.lc
enum Res<T> { Ok(T), Err(Error) }

fn load_config() Res<string> { ... }
fn validate(string cfg) Res<bool> { ... }

fn init() Res<bool> {
    auto cfg = load_config() ? { "failed to load configuration" };
    auto ok = validate(cfg) ? { "config validation failed for '{}'", cfg };
    ret Res::Ok(ok);
}
// Se load_config falhar, a mensagem inclui contexto adicionado
```

### Garantias
- [ ] `? { msg }` compila e adiciona contexto
- [ ] `? { fmt, args... }` funciona com formatação
- [ ] Mensagem original é preservada junto com o contexto
- [ ] Encadeamento múltiplo (cada nível adiciona seu contexto)
- [ ] `make test` continua passando

---

## Fase 6: Hierarquia de exceções — Múltiplos tipos no `catch`

### Problema
`catch` só captura `Error`. Gramática permite `catch (Type var)` mas IRGen ignora o tipo.

### Implementação
- **Arquivo**: `src/IRBuilder/IRGen.cpp` — `visitTryCatchStmt()`
- **Mudança**: Para cada `catchClause`, gerar comparação de tipo. Se o erro lançado corresponde ao tipo do catch, executar o bloco. Senão, tentar próximo catch.
- **Arquivo**: `src/builtins/error/error.h` — adicionar campo `type_id` ao `lucis_error` ou usar tag struct.
- **Sintaxe mantida**:
  ```lucis
  try {
      do_work();
  } catch (IOError e) {
      handle_io(e);
  } catch (ParseError e) {
      handle_parse(e);
  } catch (Error e) {
      handle_generic(e);
  }
  ```

### Testes em `ztests/`
```lucis
// ztests/src/error_multi_catch.lc
struct IOError  { message: string, code: int32 }
struct ParseError { message: string, line: int32 }

fn test_multi_catch() {
    try {
        // código que pode lançar IOError ou ParseError
    } catch (IOError e) {
        println("IO error {d}: {s}", e.code, e.message);
    } catch (ParseError e) {
        println("Parse error at line {d}: {s}", e.line, e.message);
    }
}
```

### Garantias
- [ ] Múltiplos `catch` com tipos diferentes compilam
- [ ] Dispatch de tipo funciona em runtime
- [ ] `catch (Error)` captura qualquer exceção (fallback)
- [ ] Compatível com `finally`
- [ ] `make test` continua passando

---

## Fase 7: `try` expressão com `Option<T>` — Não perder informação de erro

### Problema
`auto x = try fallible();` retorna null no erro, silenciosamente. Deveria retornar um `Option<T>` ou similar.

### Implementação
- **Opção A**: `try` expressão retorna `Result<T, Error>` em vez de null
  ```lucis
  auto result = try fallible();  // result: Result<int32, Error>
  ```
- **Opção B**: `try` expressão retorna valor ou invoca `catch` inline
  ```lucis
  auto x = try fallible() catch { default_value };
  ```
- **Recomendação**: Opção B — mais explícito e sem mudar semântica existente.
- **Arquivo**: `src/IRBuilder/IRGen.cpp` — `visitTryExpr()`
- **Mudança**: Se há `catch` após `try expr`, gerar caminho de erro com valor default do catch.

### Testes em `ztests/`
```lucis
// ztests/src/error_try_expression.lc
fn fallible() int32 { throw Error { message: "fail" }; }

fn test_try_with_default() {
    auto x = try fallible() catch { -1 };
    // x == -1 se fallible lançar exceção
}
```

### Garantias
- [ ] `try expr catch { default }` compila e funciona
- [ ] Tipo do catch block é assignable ao tipo da expressão
- [ ] `try expr` sem catch mantém comportamento atual (null)
- [ ] `make test` continua passando

---

## Fase 8: Pool allocator para `lucis_eh_frame`

### Problema
Cada `try`/`catch` faz `malloc`/`free`. Em hot paths, overhead desnecessário.

### Implementação
- **Arquivo**: `src/builtins/error/error.c`
- **Mudança**: Substituir `malloc`/`free` por pool pré-alocado:
  ```c
  #define LUCIS_EH_POOL_SIZE 16
  static __thread lucis_eh_frame eh_pool[LUCIS_EH_POOL_SIZE];
  static __thread int eh_pool_used = 0;
  ```
- Alocação: retorna `&eh_pool[eh_pool_used++]` (com bounds check, fallback pra malloc)
- Liberação: `eh_pool_used--`
- **Trade-off**: Limite de 16 frames aninhados (suficiente para uso normal)

### Testes em `ztests/`
```lucis
// ztests/src/error_pool_alloc.lc
fn test_nested_try() {
    try {
        try {
            try {
                throw Error { message: "deep" };
            } catch (Error e) {
                println("level 3: {s}", e.message);
            }
        } catch (Error e) {
            println("level 2: got re-thrown?");
        }
    } catch (Error e) {
        println("level 1");
    }
}
// Garantia: até 16 níveis de try aninhados funcionam
```

### Garantias
- [ ] Pool allocator não quebra com nesting profundo
- [ ] Fallback para malloc se pool esgotado
- [ ] Thread-safe (cada thread tem seu pool)
- [ ] Sem regressão de performance em benchmarks
- [ ] `make test` continua passando

---

## Ordem de Prioridade

| Prioridade | Fase | Impacto | Complexidade |
|-----------|------|---------|-------------|
| **1 (crítico)** | Fase 1: `defer` no `throw` | Corrige vazamento de recursos | Média |
| **2 (alto)** | Fase 3: `?` em funções `void` | Desbloqueia padrão comum | Baixa |
| **3 (alto)** | Fase 4: `#[error]` para variantes | Flexibilidade em APIs | Média |
| **4 (médio)** | Fase 2: Relaxar `?` entre enums | Composição entre módulos | Alta |
| **5 (médio)** | Fase 5: Wrapping de contexto | Debugabilidade | Média |
| **6 (baixo)** | Fase 6: Múltiplos `catch` | Completude de exceções | Alta |
| **7 (baixo)** | Fase 7: `try expr catch` | Consistência | Média |
| **8 (otimização)** | Fase 8: Pool allocator | Performance | Baixa |

---

## Estrutura de Testes em `ztests/`

```
ztests/
├── lucis.yaml
└── src/
    ├── main.lc                          # test runner principal
    ├── error_defer_throw.lc             # Fase 1
    ├── error_compatible_enums.lc        # Fase 2
    ├── error_void_propagate.lc          # Fase 3
    ├── error_custom_variant.lc          # Fase 4
    ├── error_context_wrap.lc            # Fase 5
    ├── error_multi_catch.lc             # Fase 6
    ├── error_try_expression.lc          # Fase 7
    ├── error_pool_alloc.lc              # Fase 8
    └── lib/
        └── ops.lc                       # shared para Fase 2 (cross-file)
```

Cada arquivo `.lc` é um módulo independente com funções `test_*()`.
O `main.lc` importa e chama todos os testes sequencialmente.

---

## Garantias Transversais (a cada fase)

- [ ] `make build` compila sem warnings
- [ ] `lucis run` no diretório `ztests/` executa sem erros
- [ ] `lucis run tests/main.lc -q` mantém 66/66 testes passando
- [ ] Nenhum vazamento de memória detectado (rodar com `-fsanitize=address`)
- [ ] Documentação em `docs/language/error-handling.md` atualizada com novas features
- [ ] `releases/v0.0.2-beta.md` atualizado com as melhorias
