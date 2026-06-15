# Plano de Implementação: Remoção de `namespace` e Imports Path-based

## Visão Geral

Remover a declaração `namespace X;` obrigatória de todo `.lc` e tornar o sistema de `use` resolvido por caminho de arquivo (igual TypeScript/JavaScript). O compilador **nunca mais** scaneia diretórios; ele só abre arquivos quando encontra um `use` que os referencia.

```
// Hoje:
namespace Main;
use stdio::printf;

// Novo:
use stdio::printf;
```

```
// Resolução:
use src::lexer::lexer::Token;
  → procura src/lexer/lexer.lc no dir raiz do projeto
  → se não achar, procura no stdlib dir
  → abre e parseia só esse arquivo
```

---

## Checklist de Implementação

### Fase 0: Preparação (backup + baseline)

- [ ] 0.1 Criar branch `feature/path-based-imports`
- [ ] 0.2 Rodar `make test` e confirmar que tudo passa (baseline)
- [ ] 0.3 Backup de todos os `.lc` de teste (cp -r tests tests.bak, ztests → ztests.bak, stdlib → stdlib.bak)

### Fase 1: Gramática (ANTLR)

**Arquivos:** `grammar/LucisLexer.g4`, `grammar/LucisParser.g4`

- [ ] 1.1 Remover `NAMESPACE : 'namespace';` de `LucisLexer.g4`
- [ ] 1.2 Remover regra `namespaceDecl` de `LucisParser.g4`
- [ ] 1.3 Mudar `program` de `namespaceDecl? preambleDecl* topLevelDecl* EOF` para `preambleDecl* topLevelDecl* EOF`
- [ ] 1.4 Atualizar comentários no grammar que mencionam namespace
- [ ] 1.5 Regenerar ANTLR: `make grammar`
- [ ] 1.6 Remover `grammar/.antlr/` (cache obsoleto, será recriado)

### Fase 2: ModuleRegistry (substitui NamespaceRegistry)

**Arquivos:** `src/namespace/NamespaceRegistry.h`, `src/namespace/NamespaceRegistry.cpp`, `src/namespace/ProjectScanner.h`, `src/namespace/ProjectScanner.cpp`

- [ ] 2.1 Renomear `NamespaceRegistry` → `ModuleRegistry`
  - `ExportedSymbol` mantém, mas `namespaceName` vira `modulePath` (o path relativo do arquivo)
  - `namespaces_` vira `modules_`, chave é module path (ex: `"src/lexer/lexer"`)
  - `registerFile(path, tree)` usa o path como chave, não extrai namespace do tree
- [ ] 2.2 Remover `isStdModule()` de `ModuleRegistry` (vai pra `ImportResolver`)
- [ ] 2.3 `mangle(ns, name)` → `mangle(path, name)`: ex `"src_lexer_lexer__Token"`
  - Path com `::` substituído por `__` (ex: `src::lexer::lexer` → `src_lexer_lexer`)
  - Anexar `__NomeSimbolo`
- [ ] 2.4 `validate()`: remove validação de namespace duplicado
- [ ] 2.5 `findSymbol(modulePath, name)`: lookup pelo path do módulo
- [ ] 2.6 `ProjectScanner` vira helper de `findFile(importPath, searchDirs)`:
  - Converte `use src::lexer::lexer::Token` → path `src/lexer/lexer.lc`
  - Procura em: (1) projectRoot, (2) sourcePaths do lucis.yaml, (3) stdlib dir
  - Remove `scan()` — não scaneia mais diretórios
- [ ] 2.7 Remover `getNamespaceSymbols` → `getModuleSymbols(path)`
- [ ] 2.8 Remover `getExternalSymbols(ns, excludeFile)` → `getExports(modulePath)`
- [ ] 2.9 Remover `allNamespaces()` → `allModules()`
- [ ] 2.10 Remover `hasNamespace(ns)` → `hasModule(path)`
- [ ] 2.11 Atualizar `CMakeLists.txt` com os novos nomes de arquivo (se renomear)

### Fase 3: ImportResolver (stdlib)

**Arquivos:** `src/imports/ImportResolver.h`, `src/imports/ImportResolver.cpp`

- [ ] 3.1 Mover `isStdModule` de `NamespaceRegistry` para `ImportResolver::isStdPath(path)`
- [ ] 3.2 Remover `knownModules_` (tabela de símbolos) — stdlib será resolvida por path
- [ ] 3.3 Adicionar `stdlibPath()` → retorna `$PREFIX/share/lucis/stdlib/`
- [ ] 3.4 `resolve(symbol, typeSuffix)` mantém — ainda mapeia símbolos C

### Fase 4: Pipeline (compilação sob demanda)

**Arquivos:** `src/cli/LucisPipeline.h`, `src/cli/LucisPipeline.cpp`

- [ ] 4.1 `struct CompileUnit`:
  - Remove `namespaceName`
  - Adiciona `modulePath` (ex: `"src/lexer/lexer"`)
- [ ] 4.2 Remover `extractNamespace(tree)` — não precisa mais
- [ ] 4.3 **Novo fluxo de descoberta:**
  - Parseia entry point (main.lc)
  - Extrai `use` declarations
  - BFS: pra cada `use`, resolve path via `findFile()`, parseia, extrai `use` dele
  - Repete até não ter mais imports não visitados
  - `visited` set previne ciclos
- [ ] 4.4 **Busca de entry point sem argumento:**
  - `resolveInputFile` (RunCommand, BuildCommand):
  - Procura `main.lc` no CWD, depois `src/main.lc`
  - Ou lê `lucis.yaml → entrypoint`
  - **Remove** a busca por `namespace Main`
- [ ] 4.5 `std::string resolveModulePath(const std::string& usePath, const std::vector<std::string>& searchDirs)`
  - Converte `use stdio::printf` → procura `stdio.lc` nos search dirs
  - Converte `use src::lexer::lexer::Token` → procura `src/lexer/lexer.lc`
  - Retorna path absoluto ou erro
- [ ] 4.6 Remover Step 4 (namespace registry) — substituir por ModuleRegistry
- [ ] 4.7 `registry->registerFile` recebe module path (derivado do file path) + tree
- [ ] 4.8 Remover scan de stdlib (não scaneia mais diretório) — usa tabela de lookup

### Fase 5: CLI — Comandos

**Arquivos:** `src/cli/RunCommand.cpp`, `src/cli/BuildCommand.cpp`, `src/cli/CheckCommand.cpp`, `src/cli/InitCommand.cpp`, `src/cli/CLI.h`, `src/cli/ArgParser.cpp`

- [ ] 5.1 `RunCommand::resolveInputFile()`:
  - Remove `content.find("namespace Main")`
  - Procura `main.lc` ou lê `lucis.yaml`
- [ ] 5.2 `BuildCommand::resolveInputFile()`:
  - Mesma lógica
- [ ] 5.3 `InitCommand`:
  - Template do `main.lc` não tem mais `namespace Main;`
  - Começa direto com `fn main() int32 { ... }`
- [ ] 5.4 `IrGen` chamado com `setModuleContext` em vez de `setNamespaceContext`
- [ ] 5.5 Nome do arquivo objeto (`.o`) de `namespaceName__symbol` → `modulePath__symbol` (com `::` → `_`)

### Fase 6: Checker

**Arquivos:** `src/checkers/Checker.h`, `src/checkers/Checker.cpp`

- [ ] 6.1 `setNamespaceContext` → `setModuleContext(ModuleRegistry*, modulePath, filePath)`
- [ ] 6.2 `checkUseDecls`:
  - `use Root;` → resolve path `Root.lc`
  - `use path::to::module::Symbol;` → resolve path `path/to/module.lc`, busca `Symbol` nos exports
  - `use path::to::module::{A, B};` → mesmo, mas importa múltiplos símbolos
  - `use Mod::*;` (enum wildcard) → resolve módulo, importa todos os enum variants
- [ ] 6.3 `userImports_`: chave vira o symbol name → valor vira o caminho completo do módulo exportador
- [ ] 6.4 Toda referência a `nsRegistry_` → `moduleRegistry_`:
  - `nsRegistry_->findSymbol(currentNamespace_, name)` → `moduleRegistry_->findSymbol(currentModulePath_, name)`
  - `nsRegistry_->getExternalSymbols(currentNamespace_, currentFile_)` → `moduleRegistry_->getExports(currentModulePath_)`
  - `nsRegistry_->hasNamespace(ns)` → `moduleRegistry_->hasModule(modulePath)`
  - `currentNamespace_` → `currentModulePath_`
- [ ] 6.5 `mangleGenericName`: usa module path em vez de namespace
- [ ] 6.6 `checkEnumWildcardUse` (use Type::*): resolve type via module path

### Fase 7: IRGen

**Arquivos:** `src/IRBuilder/IRGen.h`, `src/IRBuilder/IRGen.cpp`

- [ ] 7.1 `setNamespaceContext` → `setModuleContext`
- [ ] 7.2 `nsRegistry_` → `moduleRegistry_`, `currentNamespace_` → `currentModulePath_`
- [ ] 7.3 `NamespaceRegistry::mangle(...)` → `ModuleRegistry::mangle(...)`
- [ ] 7.4 `registerCrossFileSymbols`:
  - `nsRegistry_->getExternalSymbols(currentNamespace_, currentFile_)` → `moduleRegistry_->getExports(currentModulePath_)`
  - `for (auto& [symName, ns] : userImports_)` → lookup por module path
- [ ] 7.5 `declareExtendMethods(currentNamespace_)` → `declareExtendMethods(currentModulePath_)`
- [ ] 7.6 Mangling de nomes de função:
  - `NamespaceRegistry::mangle(currentNamespace_, funcName)` → `ModuleRegistry::mangle(currentModulePath_, funcName)`
  - `"ns__func"` → `"path_to_module__func"` (ex: `"src_lexer_lexer__tokenize"`)
- [ ] 7.7 `resolveExprTypeInfo`: referências a `nsRegistry_` → `moduleRegistry_`

### Fase 8: LSP

**Arquivos:** `src/lsp/ProjectContext.h`, `src/lsp/ProjectContext.cpp`, `src/lsp/DefinitionProvider.cpp`, `src/lsp/CompletionProvider.cpp`, `src/lsp/HoverProvider.cpp`, `src/lsp/SignatureHelpProvider.cpp`, `src/lsp/SemanticTokensProvider.cpp`, `src/lsp/LspServer.cpp`

#### 8.1 ProjectContext
- [ ] 8.1.1 `extractNamespace` → `extractModulePath` (deriva do path do arquivo)
- [ ] 8.1.2 `fileNamespaces_` → `fileModulePaths_`
- [ ] 8.1.3 `registry()` → retorna `ModuleRegistry&`
- [ ] 8.1.4 Remove scan de stdlib (usa tabela estática)
- [ ] 8.1.5 BFS de imports (igual pipeline)

#### 8.2 DefinitionProvider
- [ ] 8.2.1 Toda iteração `allNamespaces()` → `allModules()`
- [ ] 8.2.2 `getNamespaceSymbols(ns)` → `getModuleSymbols(path)`
- [ ] 8.2.3 `findSymbol(ns, name)` → `findSymbol(path, name)`
- [ ] 8.2.4 Goto-definition de use path → abre o arquivo correspondente

#### 8.3 CompletionProvider
- [ ] 8.3.1 `allNamespaces()` → `allModules()`
- [ ] 8.3.2 `hasNamespace(ns)` → `hasModule(path)`
- [ ] 8.3.3 Auto-complete de `use path::to::` → sugere módulos disponíveis no diretório
- [ ] 8.3.4 `isStdModule` → `ImportResolver::isStdPath`

#### 8.4 HoverProvider
- [ ] 8.4.1 `allNamespaces()` → `allModules()`
- [ ] 8.4.2 `getNamespaceSymbols(ns)` → `getModuleSymbols(path)`
- [ ] 8.4.3 Hover em símbolo importado → mostra módulo de origem

#### 8.5 SignatureHelpProvider
- [ ] 8.5.1 `allNamespaces()` → `allModules()`

#### 8.6 SemanticTokensProvider
- [ ] 8.6.1 Remove highlight de `namespace` keyword (grammar já não tem)

#### 8.7 LspServer
- [ ] 8.7.1 `content.find("namespace Main")` → `content.find("fn main(")`
- [ ] 8.7.2 Resolução de entry point sem namespace

### Fase 9: Intrinsics (`lucis::sys`, etc.)

**Arquivos:** `src/intrinsics/IntrinsicRegistry.h`, `src/intrinsics/IntrinsicRegistry.cpp`, e todos os `*Namespace.cpp`

- [ ] 9.1 Intrinsics **não mudam** — continuam sendo `lucis::sys::read`, etc.
- [ ] 9.2 `IntrinsicRegistry` é separado de `ModuleRegistry` e não é afetado
- [ ] 9.3 Apenas verificar que `isIntrinsicPrefix` e `parseIntrinsicPath` continuam funcionando
- [ ] 9.4 Nenhuma mudança necessária nos namespaces de intrinsics

### Fase 10: Arquivos `.lc` de Teste

**Arquivos:** `tests/*.lc`, `ztests/src/main.lc`, `stdlib/stdio.lc`

- [ ] 10.1 Remover `namespace Main;` de TODOS os `tests/*.lc` (~50 arquivos)
- [ ] 10.2 Remover `namespace Main;` de `ztests/src/main.lc`
- [ ] 10.3 Remover `namespace stdio;` de `stdlib/stdio.lc`
- [ ] 10.4 Ajustar `use` se necessário (testes que usam `use stdio::printf`)
- [ ] 10.5 Rodar `make test` e corrigir erros

### Fase 11: Teste Unitário (TestCommand/ztests)

**Arquivos:** `src/cli/TestCommand.cpp`

- [ ] 11.1 Remover dependência de namespace no test runner
- [ ] 11.2 Ajustar lógica de descoberta de entry point

### Fase 12: Docs

**Arquivos:** `docs/language/namespaces.md`, `docs/language/syntax.md`, `docs/language/overview.md`, `docs/language/modules.md`, `docs/language/variables.md`, `docs/language/functions.md`, `docs/language/control-flow.md`, `docs/language/operators.md`, `docs/language/types.md`, `docs/language/error-handling.md`, `docs/language/concurrency.md`, `docs/language/doc-comments.md`, `docs/language/intrinsics.md`, `docs/language/intrinsics/sys.md`, `docs/language/intrinsics/unsafe.md`, `docs/language/intrinsics/io.md`, `docs/language/intrinsics/atomic.md`, `docs/reference/keywords.md`, `docs/reference/grammar.md`, `docs/reference/changelog.md`, `docs/getting-started/hello-world.md`, `docs/getting-started/cli-usage.md`, `docs/ffi/overview.md`, `docs/ffi/structs-abi.md`, `docs/ffi/linking.md`, `docs/ffi/README.md`, `docs/advanced/intrinsics.md`, `docs/advanced/compiler-internals.md`, `docs/advanced/extending.md`

- [ ] 12.1 Remover/revisar `docs/language/namespaces.md` (vira `docs/language/modules.md`)
- [ ] 12.2 Atualizar `docs/language/syntax.md`:
  - `namespace X;` não é mais obrigatório
  - `use` resolve por path, não por namespace
  - Ordem: `use` + `#include` antes de declarações
- [ ] 12.3 Atualizar todos os code examples nos `.md`:
  - Remover `namespace X;` de todos os exemplos
  - Ajustar `use` paths se necessário
- [ ] 12.4 `docs/reference/keywords.md`: remover `namespace`
- [ ] 12.5 `docs/reference/grammar.md`: atualizar BNF
- [ ] 12.6 `docs/reference/changelog.md`: adicionar entrada
- [ ] 12.7 `docs/getting-started/hello-world.md`: reescrever seção de namespace
- [ ] 12.8 `docs/getting-started/cli-usage.md`: atualizar descrição do pipeline
- [ ] 12.9 `docs/advanced/compiler-internals.md`: atualizar seção NamespaceRegistry
- [ ] 12.10 `docs/language/modules.md`: reescrever para path-based

### Fase 13: Release Notes

**Arquivos:** `releases/v0.0.2-beta.md`

- [ ] 13.1 Adicionar seção "Breaking: Namespace Removed"
- [ ] 13.2 Descrever novo sistema path-based
- [ ] 13.3 Instruções de migração (remover `namespace X;`, ajustar `use` paths)

### Fase 14: VS Code Extension

**Arquivos:** `editors/vscode-lucis/syntaxes/lucis.tmLanguage.json`

- [ ] 14.1 Remover regras de syntax highlight para `namespace` keyword
- [ ] 14.2 Remover regra `entity.name.namespace.lucis`
- [ ] 14.3 Atualizar keyword list (remover `namespace`)

### Fase 15: Builtins (C)

**Arquivos:** `src/builtins/**/*.c`, `src/builtins/**/*.h`

- [ ] 15.1 Nenhuma mudança — C builtins são externas e não usam namespace Lucis
- [ ] 15.2 Apenas verificar que o mangling de nomes continua funcionando (builtins são chamados por nome C direto)

### Fase 16: Config

**Arquivos:** `src/config/LucisConfig.h`

- [ ] 16.1 `sourcePaths`: mantém, ainda usado pra busca de arquivos
- [ ] 16.2 `entrypoint`: pode especificar path relativo (ex: `src/main.lc`)

### Fase 17: Limpeza

- [ ] 17.1 Remover `src/namespace/` directory (ProjectScanner + NamespaceRegistry substituídos)
- [ ] 17.2 Remover `docs/_ignore/` (planejamentos obsoletos)
- [ ] 17.3 Rodar `make test` completo e confirmar 0 falhas
- [ ] 17.4 Rodar LSP manualmente com alguns projetos de teste
- [ ] 17.5 Verificar que `lucis run` sem argumento funciona (acha main.lc)
- [ ] 17.6 Verificar que `lucis init` gera main.lc sem namespace
- [ ] 17.7 Verificar que stdlib só compila quando importado
- [ ] 17.8 Verificar que `use stdio::printf` funciona
- [ ] 17.9 Verificar que `use src::mod::Sub` funciona com path relativo
- [ ] 17.10 Verificar que ciclo de import é detectado (erro)

---

## Resumo de Arquivos Afetados

| Componente | Arquivos | Esforço |
|-----------|----------|---------|
| Gramática | 2 | ~15min |
| ANTLR generated | ~10 | automático |
| ModuleRegistry | 2 | ~2h |
| ImportResolver | 2 | ~30min |
| Pipeline/CLI | 6 | ~3h |
| Checker | 2 | ~3h |
| IRGen | 2 | ~2h |
| LSP | 8 | ~4h |
| Testes (.lc) | ~52 | ~4h |
| Docs (.md) | ~28 | ~3h |
| Release notes | 1 | ~15min |
| VS Code ext | 1 | ~15min |
| Config | 1 | ~5min |
| **Total** | **~118** | **~22h** |

## Ordem de Implementação Recomendada

```
Fase 0 (backup)
  → Fase 1 (grammar) + Fase 2 (ModuleRegistry)
  → Fase 3 (ImportResolver)
  → Fase 4 (pipeline)
  → Fase 5 (CLI)
  → Fase 6 (checker) + Fase 7 (IRGen) [podem ser paralelos]
  → Fase 8 (LSP)
  → Fases 9-11 (intrinsics, testes, test command)
  → Fases 12-14 (docs, releases, vscode)
  → Fase 15-17 (builtins, config, limpeza)
```

Cada fase deve ser testada com `cmake --build build && make test` antes de prosseguir.
