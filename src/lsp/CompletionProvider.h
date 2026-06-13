#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "generated/LucisParser.h"
#include "ffi/CBindings.h"
#include "types/BuiltinRegistry.h"
#include "types/TypeRegistry.h"
#include "types/MethodRegistry.h"
#include "types/ExtendedTypeRegistry.h"
#include "intrinsics/IntrinsicRegistry.h"
#include "imports/ImportResolver.h"

struct ParseResult;
class ProjectContext;

// LSP CompletionItemKind values
enum class CompletionKind : int {
    Text          = 1,
    Method        = 2,
    Function      = 3,
    Constructor   = 4,
    Field         = 5,
    Variable      = 6,
    Class         = 7,   // struct
    Interface     = 8,
    Module        = 9,
    Property      = 10,
    Unit          = 11,
    Value         = 12,
    Enum          = 13,
    Keyword       = 14,
    Snippet       = 15,
    Color         = 16,
    File          = 17,
    Reference     = 18,
    Folder        = 19,
    EnumMember    = 20,
    Constant      = 21,
    Struct        = 22,
    Event         = 23,
    Operator      = 24,
    TypeParameter = 25,
};

// LSP InsertTextFormat
enum class InsertTextFormat : int {
    PlainText = 1,
    Snippet   = 2,
};

// A single completion suggestion.
struct CompletionItem {
    std::string      label;
    CompletionKind   kind = CompletionKind::Text;
    std::string      detail;           // short type/signature info
    std::string      documentation;    // longer markdown description
    std::string      insertText;       // text to insert (if different from label)
    InsertTextFormat insertTextFormat = InsertTextFormat::PlainText;
    std::string      sortText;         // controls ordering (empty = use label)
    std::string      filterText;       // controls filtering (empty = use label)
};

// Provides autocompletion for Lucis source code.
class CompletionProvider {
public:
    CompletionProvider();

    // Returns completions at the given 0-based line:col.
    std::vector<CompletionItem> complete(
        const std::string& source, size_t line, size_t col,
        const std::string& filePath, const ProjectContext* project,
        ParseResult* preParsed = nullptr);

    // Pre-warm C header cache (call early to avoid first-completion delay).
    void warmHeaderCache();

    // Lightweight variable info.
    struct LocalVar {
        std::string typeName;
        unsigned    arrayDims = 0;
    };

private:
    BuiltinRegistry builtinRegistry_;
    TypeRegistry typeRegistry_;
    MethodRegistry methodRegistry_;
    ExtendedTypeRegistry extTypeRegistry_;
    IntrinsicRegistry intrinsicRegistry_;
    bool hasIncludeBindingsCache_ = false;
    std::string includeFingerprintCache_;
    CBindings includeBindingsCache_;

    // ── Context detection ───────────────────────────────────────────

    enum class CompletionContext {
        General,        // bare identifier — show everything in scope
        DotAccess,      // expr.| — show struct fields + extend methods
        ArrowAccess,    // expr->| — same as dot but for pointers
        ScopeAccess,    // Name::| — show enum variants or static methods
        TypePosition,   // type annotation — show types only
        IncludeHeader,  // #include <| — show available C headers
        UseImport,      // use path::| — show modules and symbols
        DocComment,     // inside /** ... */ — show doc-tags
    };

    struct CompletionRequest {
        CompletionContext context = CompletionContext::General;
        std::string prefix;          // partial identifier typed so far
        std::string receiverType;    // for dot/arrow: struct type name
        std::string receiverVar;     // for dot/arrow: receiver variable name (resolve later)
        std::string receiverCall;    // for dot: receiver function call name (e.g. foo().|)
        std::string scopeName;       // for Name::| or A::B::| — scope path
        std::string modulePath;      // for use: accumulated path (e.g. "std::log")
        unsigned    indexDepth = 0;   // for dot: number of [..] index accesses on receiver
        unsigned    derefDepth = 0;   // for dot: number of explicit unary * before receiver
        std::vector<std::string> methodChain; // for dot: chained members (fields/methods), e.g. {"message", "len"}
        bool closingCharPresent = false; // for IncludeHeader: '>' or '"' already exists after cursor
    };

    // Analyze the source text around the cursor to determine context.
    CompletionRequest analyzeContext(const std::string& source,
                                    size_t line, size_t col,
                                    LucisParser::ProgramContext* tree);

    // ── Collectors ──────────────────────────────────────────────────

    // Add locals + params in scope at cursor position.
    void addLocals(std::vector<CompletionItem>& items,
                   LucisParser::ProgramContext* tree, size_t cursorLine,
                   const CBindings& bindings,
                   const std::string& prefix);

    // Add same-file top-level declarations (functions, structs, enums, etc).
    void addLocalDecls(std::vector<CompletionItem>& items,
                       LucisParser::ProgramContext* tree,
                       const std::string& prefix);

    // Add imported symbols from `use` declarations.
    void addImportedSymbols(std::vector<CompletionItem>& items,
                            LucisParser::ProgramContext* tree,
                            const ProjectContext* project,
                            const std::string& prefix);

    // Add enum variant names from `use EnumType::*;` declarations.
    void addEnumWildcardVariants(std::vector<CompletionItem>& items,
                                 LucisParser::ProgramContext* tree,
                                 const ProjectContext* project,
                                 const std::string& prefix,
                                 size_t cursorLine);

    // Add cross-file symbols from the project registry.
    void addProjectSymbols(std::vector<CompletionItem>& items,
                           const ProjectContext* project,
                           const std::string& filePath,
                           const std::string& prefix);

    // Add C symbols (functions, structs, enums, macros, globals).
    void addCSymbols(std::vector<CompletionItem>& items,
                     const CBindings& bindings,
                     const std::string& prefix);

    // Add struct fields for dot/arrow access.
    void addStructFields(std::vector<CompletionItem>& items,
                         const std::string& structName,
                         LucisParser::ProgramContext* tree,
                         const CBindings& bindings,
                         const ProjectContext* project);

    // Add extend methods for dot/arrow access.
    void addExtendMethods(std::vector<CompletionItem>& items,
                          const std::string& typeName,
                          LucisParser::ProgramContext* tree,
                          const ProjectContext* project);

    // Add built-in type methods (int.abs, string.len, vec.push, etc.).
    void addTypeMethods(std::vector<CompletionItem>& items,
                        const std::string& typeName,
                        const std::string& prefix);

    // Add enum variants for scope (::) access.
    void addEnumVariants(std::vector<CompletionItem>& items,
                         const std::string& enumName,
                         LucisParser::ProgramContext* tree,
                         const CBindings& bindings,
                         const ProjectContext* project,
                         const std::string& prefix);

    // Add static methods for scope (::) access.
    void addStaticMethods(std::vector<CompletionItem>& items,
                          const std::string& typeName,
                          LucisParser::ProgramContext* tree,
                          const ProjectContext* project,
                          const std::string& prefix);

    // Add type names (for type annotation positions).
    void addTypeNames(std::vector<CompletionItem>& items,
                      LucisParser::ProgramContext* tree,
                      const CBindings& bindings,
                      const ProjectContext* project,
                      const std::string& prefix);

    // Add language keywords.
    void addKeywords(std::vector<CompletionItem>& items,
                     const std::string& prefix);

    // Add use import path completions (modules and symbols).
    void addUseCompletions(std::vector<CompletionItem>& items,
                           const std::string& modulePath,
                           const std::string& prefix,
                           const ProjectContext* project);

    // Add global builtins (assert, panic, toString, etc.).
    void addGlobalBuiltins(std::vector<CompletionItem>& items,
                            const std::string& prefix);

    // Add intrinsic functions (lucis::core::trap, etc.).
    void addIntrinsics(std::vector<CompletionItem>& items,
                       const std::string& scopeName,
                       const std::string& prefix);

    // Add the 'lucis' intrinsic root in general context.
    void addIntrinsicRoot(std::vector<CompletionItem>& items,
                          const std::string& prefix);

    // Add C header file suggestions for #include <| or #include "|".
    void addHeaderSuggestions(std::vector<CompletionItem>& items,
                              const std::string& prefix,
                              bool omitClosingChar,
                              const std::string& filePath,
                              const ProjectContext* project);

    // Add doc-tag completions inside /** ... */ blocks.
    void addDocTagCompletions(std::vector<CompletionItem>& items,
                              const std::string& prefix);

    // ── Helpers ─────────────────────────────────────────────────────

    // Resolve the return type of a method call on a given receiver type.
    // Used for method chaining: x.abs().toString() → resolve return type at each step.
    std::string resolveMethodReturnType(
        const std::string& receiverType, const std::string& methodName,
        LucisParser::ProgramContext* tree, const ProjectContext* project);

    // Resolve the type of a field access on a given receiver type.
    std::string resolveFieldType(
        const std::string& receiverType, const std::string& fieldName,
        LucisParser::ProgramContext* tree, const CBindings* bindings,
        const ProjectContext* project);

    // Collect locals from a function body.
    std::unordered_map<std::string, LocalVar>
    collectLocals(LucisParser::FunctionDeclContext* func, size_t beforeLine,
                  LucisParser::ProgramContext* tree = nullptr,
                  const CBindings* bindings = nullptr,
                  const ProjectContext* project = nullptr);

    // Collect locals from an extend method body.
    std::unordered_map<std::string, LocalVar>
    collectLocalsFromMethod(LucisParser::ExtendMethodContext* method, size_t beforeLine);

    // Find enclosing function at a line.
    LucisParser::FunctionDeclContext*
    findEnclosingFunction(LucisParser::ProgramContext* tree, size_t line);

    // Find enclosing extend method at a line.
    LucisParser::ExtendMethodContext*
    findEnclosingMethod(LucisParser::ProgramContext* tree, size_t line);

    // Find struct/enum/extend declarations by name.
    LucisParser::StructDeclContext*
    findStructDecl(LucisParser::ProgramContext* tree, const std::string& name);

    LucisParser::UnionDeclContext*
    findUnionDecl(LucisParser::ProgramContext* tree, const std::string& name);

    LucisParser::EnumDeclContext*
    findEnumDecl(LucisParser::ProgramContext* tree, const std::string& name);

    LucisParser::ExtendDeclContext*
    findExtendDecl(LucisParser::ProgramContext* tree, const std::string& name);

    // Infer struct type from a variable name at cursor line.
    std::string inferVarType(const std::string& varName,
                             LucisParser::ProgramContext* tree,
                             size_t cursorLine,
                             const CBindings* bindings = nullptr,
                             const ProjectContext* project = nullptr);

    // Format a function signature for display.
    static std::string formatFuncSignature(LucisParser::FunctionDeclContext* func);

    // Format an extend method signature for display.
    static std::string formatMethodSignature(LucisParser::ExtendMethodContext* method);

    // Format a typeSpec as human-readable string.
    static std::string typeSpecToString(LucisParser::TypeSpecContext* ts);

    // Check if a string starts with a prefix (case-insensitive).
    static bool matchesPrefix(const std::string& name, const std::string& prefix);

    // Dedup items by label.
    static void dedup(std::vector<CompletionItem>& items);

    // Build a stable key from include declarations for C binding cache.
    static std::string buildIncludeFingerprint(LucisParser::ProgramContext* tree);
};
