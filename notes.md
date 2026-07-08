# Language Improvements — Implementation Plan

Ordem proposta: ~~Destrutor automático~~ → **Iterator → Operator Overloading → Traits → Constraints**
Cada um entrega valor sozinho e prepara terreno pro próximo.

---

## ~~1. Destrutor Automático (2-4 dias)~~ ✅ Concluído

Ver commit `9ef92aa`.

### O que é
Invocar `drop()` automaticamente ao sair do escopo para toda variável local cujo tipo possui método `drop()`, na ordem inversa de declaração — semelhante ao RAII de C++.

### Onde mexer

**Checker (`src/checkers/Checker.cpp`):**
- Já reconhece `drop()` em `checkExtendDecl` e marca `hasDrop = true` no TypeInfo
- Já tem `dropTracked` em `LocalVarInfo`
- Garantir que ao declarar variável local com tipo que tem `drop()`, ela seja marcada corretamente

**IRGen (`src/IRBuilder/IRGen.cpp`):**
- `FunctionContext` já tem `varDropStack` — usado para vec built-in
- Generalizar: para cada variável local com `drop()`, inserir chamada `var.drop()` nos exit points:
  - `return` statement (antes de retornar)
  - Fim de bloco `}`
  - `break` / `continue` quando aplicável
- A ordem deve ser inversa à declaração (LIFO)

**LSP (`src/lsp/`):**
- `DiagnosticEngine` e `SemanticTokensProvider`: nenhuma mudança direta — destrutor automático não altera sintaxe
- `CompletionProvider`: sugestão de `drop()` em `extend` blocks já funciona
- `HoverProvider`: mostrar que um tipo tem destrutor automático seria um plus (info extra no TypeInfo)

### Como testar
- `tests/drop_auto.lc`:
  - Struct com `drop()` que incrementa contador global
  - Alocar em escopo aninhado, verificar se drop foi chamado ao sair
  - Alocar múltiplas vars, verificar ordem inversa
  - Testar com `return` prematuro
  - Testar com `defer` coexistindo

---

## 2. Iterator Customizado (3-5 dias)

### O que é
Permitir `for x in expr` onde `expr` é qualquer tipo que exponha método `next() -> Option<T>` (ou protocolo similar). Sem traits, o checker reconhece padronizando por convenção de nome.

### Onde mexer

**Grammar (`grammar/LucisParser.g4`):**
- Nenhuma mudança — `for x in expr` já existe na gramática

**Checker (`src/checkers/Checker.cpp`):**
- Em `checkForInStmt` (ou onde o `for-in` é validado), quando o tipo de `expr` não for vec/array/range:
  - Procurar método `next()` no tipo via `resolveMethod`
  - Verificar se retorna `Option<T>` (ou enum de 2 variantes onde uma é unit)
  - Se sim, aceitar; se não, erro "type does not support iteration"
- Resolver o tipo do elemento (`T` de `Option<T>`)

**IRGen (`src/IRBuilder/IRGen.cpp`):**
- `visitForStmt`/`visitForInStmt` (linhas ~7036-7125): adicionar branch para custom iterator
- Gerar loop:
  ```
  while (true) {
      auto opt = iter.next();
      if (opt is Option<T>::Nothing) break;
      T x = opt as Option<T>::Some;
      // corpo do loop
  }
  ```
- Alternativa: gerar chamada a `__iterator_next` como builtin (mais performático)

**LSP:**
- `CompletionProvider`: ao digitar em `for x in `<expr>`.` , sugerir `.next()` se o tipo for iterável
- `HoverProvider`: mostrar que o tipo implementa iteração
- `SignatureHelpProvider`: para `next()`, mostrar `fn next() Option<T>`
- `DiagnosticEngine`: erro "type does not support iteration" mapeado corretamente

### Como testar
- `tests/iterator_custom.lc`:
  - Struct `Counter` com método `next() -> Option<int32>` que conta até N
  - `for int32 x in Counter::new(5) { ... }` funcionando
  - Tipo sem `next()` tentando usar em for → erro de compilação
  - `next()` retornando tipo não-Option → erro

---

## 3. Operator Overloading (4-7 dias)

### O que é
Permitir que tipos do usuário definam comportamento para operadores (`+`, `-`, `*`, `/`, `[]`, etc.) através de métodos com nomes especiais em `extend` blocks.

### Onde mexer

**Grammar (`grammar/LucisParser.g4`):**
- Adicionar produção em `extendMember` ou criar `operatorDecl`:
  ```
  operatorDecl
      : OPERATOR operatorSymbol LPAREN paramList RPAREN (COLON typeSpec)? block
      ;
  ```
- `operatorSymbol`: `PLUS`, `MINUS`, `STAR`, `SLASH`, `LBRACKET RBRACKET`, etc.
- Novo token `OPERATOR` no lexer (ou reuso de `IDENTIFIER` com prefixo)

**Checker (`src/checkers/Checker.cpp`):**
- Validar declaração de operador (assinatura correta, aridade)
- Em `checkBinaryExpr` / `checkUnaryExpr` / `checkSubscriptExpr`:
  - Se os operandos não tiverem operação nativa, procurar `operator+`, `operator[]` etc. no tipo
  - Resolver e verificar tipos
- Registrar operadores como métodos especiais no TypeInfo

**IRGen (`src/IRBuilder/IRGen.cpp`):**
- Onde operações binárias/unárias são geradas (e.g. `visitAddSubExpr`, `visitMulExpr`):
  - Se não for operação nativa, gerar chamada de método `a.op_binary_add(b)` ou similar
- Subscript: `visitSubscriptExpr` — gerar `a.op_subscript(index)` se não for array nativo

**LSP:**
- `CompletionProvider`: sugerir operadores em `extend` blocks
- `SemanticTokensProvider`: destacar `operator+` como keyword especial
- `SignatureHelpProvider`: para `a + b`, se for operator overload, mostrar assinatura do método
- `DiagnosticEngine`: validar assinaturas, relatar ambiguidades

### Como testar
- `tests/operator_overload.lc`:
  - Struct `Vec2` com `operator+`, `operator*` (escalar)
  - Struct ` Matrix` com `operator[]` retornando referência
  - Testar `a + b`, `a * 2`, `m[0]`
  - Erro se operador não definido para o tipo
  - Erro se assinatura do operador for inválida

---

## 4. Traits / Interfaces (2-4 semanas)

### O que é
Sistema de abstração polimórfica: declaração de trait com métodos, implementação pra tipos concretos, uso como bound em genéricos. Semelhanças com Rust traits ou Go interfaces.

### Onde mexer

**Grammar (`grammar/LucisParser.g4`):**
```
traitDecl
    : TRAIT IDENTIFIER typeParamList? LBRACE traitMember* RBRACE
    ;

traitMember
    : fnSignature SEMI
    ;

implDecl
    : IMPL typeSpec TRAIL typeSpec LBRACE extendMember* RBRACE
    ;

typeBound
    : IDENTIFIER (PLUS IDENTIFIER)*
    ;
```
- Novos tokens: `TRAIT`, `IMPL`
- Modificar `typeParam` para aceitar `IDENTIFIER (COLON typeBound)?`

**Checker (`src/checkers/Checker.cpp`):**
- `checkTraitDecl`: validar trait, registrar no `typeRegistry_`
- `checkImplDecl`: verificar se todos os métodos da trait foram implementados, verificar cobertura
- Resolver bounds em genéricos: ao instanciar `fn foo<T: Iterable>(x T)`, verificar se `T` concreto implementa `Iterable`
- Coerência: impedir impls conflitantes

**IRGen (`src/IRBuilder/IRGen.cpp`):**
- Duas abordagens possíveis:
  - **Monomorfização** (mais simples): gerar código separado pra cada combinação tipo+trait, igual genéricos normais
  - **Vtable** (mais flexível): gerar tabela de métodos e despachar indiretamente
- Começar com monomorfização, depois vtable como otimização

**LSP:**
- `CompletionProvider`: sugerir métodos da trait dentro de `impl`, sugerir traits ao digitar `:`
- `DefinitionProvider`: ir de uso de trait bound até declaração da trait
- `HoverProvider`: mostrar bounds de um parâmetro genérico
- `DiagnosticEngine`: erro se impl não cobre método da trait, erro se tipo não satisfaz bound
- `SemanticTokensProvider`: destacar `trait`, `impl` como keywords
- `SignatureHelpProvider`: mostrar métodos requeridos ao implementar trait

### Como testar
- `tests/traits_basic.lc`:
  - Trait `Drawable { fn draw() void; }`
  - Struct `Circle` impl `Drawable`, struct `Square` impl `Drawable`
  - Genérico `fn render<T: Drawable>(x T) void` chamando `x.draw()`
- `tests/traits_default.lc`: métodos com implementação default
- `tests/traits_multiple.lc`: múltiplos bounds (`T: Drawable + Clone`)

---

## 5. Custom Constraints (1-2 semanas, construído sobre Traits)

### O que é
Permitir que usuários definam constraints além das built-in (`numeric`, `integer`, etc.). Idealmente atrelado a traits: `T: Drawable` verifica se T implementa Drawable.

### Onde mexer

**Grammar:**
- Já coberto pela modificação de `typeParam` no item 4

**Checker:**
- Resolver bounds de trait em instanciação genérica
- Erro se tipo não implementa trait exigida

**IRGen:**
- Monomorfização já cobre — trait impl vira código concreto

**LSP:**
- `HoverProvider`: mostrar constraints de parâmetro genérico
- `DiagnosticEngine`: erro de bound não satisfeito com mensagem clara

### Como testar
- Junto com testes de traits (item 4)

---

---

## 6. Ownership / Move Semantics para Tipos dropTracked (Prioridade: Média)

### Contexto
Atualmente, quando um tipo `dropTracked` (como `Vec<string>`, `Map<K,V>`, `Set<T>`) é passado **por valor** como parâmetro de função, o compilador faz uma **cópia rasa** (shallow copy) da struct `{ptr, len, cap}`. O parâmetro é marcado como `isParam = true` para evitar que o cleanup automático da função callee libere a memória que o caller ainda possui.

Isso funciona, mas tem limitações:
- A função callee não pode armazenar o valor recebido sem fazer um clone manual
- Não há likeiramento de ownership: o caller sabe que ainda possui o dado
- Não é possível "transferir" ownership de forma eficiente (sem deep copy)

### O que implementar

**Move semantics ao passar por valor:**
- Quando um `dropTracked` é passado como argumento em uma chamada de função:
  - A origem é marcada como `consumed` (não libera no cleanup)
  - O destino assume ownership e libera no cleanup
- A infraestrutura já existe parcialmente:
  - `VarInfo::consumed` no IRGen
  - `consumeLocalByName()` — zera o valor e marca como consumido
  - `consumeExprIfOwnedLocal()` — detecta se a expressão é um owned local

**Deep copy automático como fallback:**
- Quando não for possível consumir a origem (e.g., referência), gerar `lucis_vec_clone_<suffix>` automaticamente
- Isso evitaria double-frees sem exigir `isParam`

**Validação no Checker:**
- Adicionar verificação: passar `dropTracked` por valor para parâmetro que não `&self` deve consumir ou marcar como moved
- Erro se a origem for usada depois do move

### Onde mexer

**IRGen (`src/IRBuilder/IRGen.cpp`):**
- `emitFnCallExpr` / `visitFnCallExpr`: ao passar argumento dropTracked por valor:
  - Se for owned local, marcar como `consumed` e passar o valor diretamente
  - Se não for owned (e.g., field access), gerar clone via `lucis_vec_clone_<suffix>`
- `isDropTrackedLocal()`: já corrigido para respeitar `isParam`

**Checker (`src/checkers/Checker.cpp`):**
- `checkFnCallExpr`: validar uso após move de dropTracked
- Adicionar lógica de "borrow checker" básica

**LSP:**
- `DiagnosticEngine`: erro "use after move" com mensagem clara
- `HoverProvider`: mostrar estado de ownership (owned / borrowed / moved)

### Como testar
- `tests/ownership_move.lc`:
  - Chamar função passando `Vec<string>` por valor, verificar que não há double-free
  - Usar a variável origem depois do move → erro de compilação
  - Passar field access (não owned) → deep copy automático
  - Strings também devem seguir o mesmo modelo

---

## Resumo de arquivos afetados

| Feature | Grammar | Checker | IRGen | LSP |
|---|---|---|---|---|
| Destrutor | — | Checker.cpp | IRGen.cpp | — |
| Iterator | — | Checker.cpp | IRGen.cpp | vários LSP |
| Operator | LucisParser.g4, LucisLexer.g4 | Checker.cpp | IRGen.cpp | vários LSP |
| Traits | LucisParser.g4, LucisLexer.g4 | Checker.cpp + TypeInfo.h | IRGen.cpp | vários LSP |
| Constraints | LucisParser.g4 | Checker.cpp | — (mono) | vários LSP |

## Ordem recomendada de implementação

```
Destrutor (2-4d) → Iterator (3-5d) → Operator (4-7d) → Traits (2-4sem) → Constraints (1-2sem)
```

Cada etapa:
1. Grammar + regenerar ANTLR
2. Checker (validação)
3. IRGen (codegen)
4. Testes em `tests/`
5. Registrar em `tests/main.lc`
6. Ajustes no LSP
7. `make test` pra garantir que nada quebrou
