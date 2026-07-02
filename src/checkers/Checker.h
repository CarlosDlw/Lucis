#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include "generated/LucisParser.h"
#include "imports/ImportResolver.h"
#include "types/TypeRegistry.h"
#include "types/MethodRegistry.h"
#include "types/ExtendedTypeRegistry.h"
#include "types/BuiltinRegistry.h"
#include "intrinsics/IntrinsicRegistry.h"
#include "comptime/ComptimeRegistry.h"
#include "namespace/ModuleRegistry.h"
#include "lsp/Diagnostic.h"
#include "semantic/SemanticDB.h"

class CBindings;
struct CFunctionLikeMacro;

class Checker {
public:
    Checker();
    bool check(LucisParser::ProgramContext* tree);

    const std::vector<std::string>& errors() const { return errors_; }
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

    // Set module context for cross-file symbol resolution.
    void setModuleContext(const ModuleRegistry* registry,
                          const std::string& modulePath,
                          const std::string& currentFile);

    // Set project directories for filesystem-based module fallback.
    void setProjectPaths(const std::string& projectRoot,
                         const std::vector<std::string>& sourcePaths);

    // Set C bindings from parsed #include headers.
    void setCBindings(const CBindings* bindings);

    // Set semantic DB for Phase 1 parallel population (alongside TypeRegistry).
    void setSemanticDB(semantic::SemanticDB* db) { semanticDB_ = db; }
    semantic::SemanticDB* semanticDB() { return semanticDB_; }

private:
    std::vector<std::string> errors_;
    std::vector<Diagnostic>  diagnostics_;
    ImportResolver           imports_;
    TypeRegistry             typeRegistry_;
    MethodRegistry           methodRegistry_;
    ExtendedTypeRegistry     extTypeRegistry_;
    BuiltinRegistry          builtinRegistry_;
    IntrinsicRegistry        intrinsicRegistry_;
    ComptimeRegistry         comptimeRegistry_;

    // Variable info tracked per-function scope
    struct VarInfo {
        enum class OwnershipState {
            Owned,
            BorrowedImm,
            BorrowedMut,
            Moved,
            Escaped,
            Dropped,
        };

        const TypeInfo* type;
        unsigned arrayDims = 0;
        bool initialized = true;  // false when declared without initializer
        bool used = false;         // set to true when the variable is read
        antlr4::Token* declToken = nullptr; // for warning location
        unsigned scopeDepth = 0;   // nesting depth at declaration point
        OwnershipState ownership = OwnershipState::BorrowedImm;

        // Phase 1: lightweight buffer tracking for pointer-based C calls
        bool hasBufferCapacity = false;
        uint64_t bufferCapacity = 0;
        bool hasKnownCStringLen = false;
        uint64_t cstringLen = 0;

        // Phase 3: lightweight range + escape tracking
        bool hasKnownUSizeRange = false;
        uint64_t minUSize = 0;
        uint64_t maxUSize = 0;
        bool pointerEscaped = false;
        bool isConst = false;  // true if declared with `const`
    };
    std::unordered_map<std::string, VarInfo> locals_;

    // Top-level const variables, persisted across function-scope clears
    std::unordered_map<std::string, VarInfo> globalVars_;

    // Labels declared in the current function (for asm goto validation)
    std::unordered_set<std::string> currentFunctionLabels_;
    // Labels referenced by asm goto (validated at function end)
    std::unordered_set<std::string> asmGotoLabelRefs_;

    unsigned scopeDepth_ = 0;     // current block nesting depth
    const TypeInfo* currentReturnType_ = nullptr;
    unsigned unwrapCatchItDepth_ = 0;
    unsigned recursionDepth_ = 0;  // recursion depth guard (max 1000 levels)
    static constexpr unsigned MAX_RECURSION_DEPTH = 1000;

    // Phase 3 (Guard Analysis): Track conditional guards to suppress false positives
    // When code has `if (n <= cap)`, we suppress buffer overflow warnings for calls
    // to unsafe functions within that if body that would use n as a bound.
    struct Guard {
        std::string varName;       // variable being checked, e.g. "n"
        std::string op;            // "<=", "<", ">=", ">"
        uint64_t threshold;        // the capacity/limit value
        
        Guard() = default;
        Guard(const std::string& vn, const std::string& o, uint64_t t)
            : varName(vn), op(o), threshold(t) {}
    };
    std::vector<std::vector<Guard>> guardStack_;  // stack of guard sets per scope

    // Extract comparison guards from an if condition expression
    // e.g. `n <= 100` → Guard{"n", "<=", 100}
    // Returns all guards found (may be multiple in && chains)
    std::vector<Guard> extractGuardsFromExpr(LucisParser::ExpressionContext* expr);

    // Query: does any active guard prove that value <= targetCapacity?
    bool queryGuard(const std::string& varName, uint64_t targetCapacity) const;

    // Module-level function signatures (name → function TypeInfo)
    std::unordered_map<std::string, const TypeInfo*> functions_;

    // Function declarations (for error reporting with location of first definition)
    std::unordered_map<std::string, LucisParser::FunctionDeclContext*> functionDecls_;

    // Global builtin names (always available, no import needed)
    std::unordered_set<std::string> globalBuiltins_;

    // Track loop nesting for break/continue validation
    unsigned loopDepth_ = 0;

    // Struct methods registered via `extend` blocks
    struct StructMethodInfo {
        std::string name;
        const TypeInfo* returnType;
        std::vector<const TypeInfo*> paramTypes; // excludes &self
        bool isStatic = false;
    };
    std::unordered_map<std::string, std::vector<StructMethodInfo>> structMethods_;

    // Walk parent chain to find a method (struct inheritance)
    const StructMethodInfo* findMethodInChain(const TypeInfo* ti,
                                               const std::string& name) const;
    // Collect ALL methods from parent chain (for completions)
    std::vector<const StructMethodInfo*> collectMethodsInChain(
        const TypeInfo* ti) const;

    // Dynamically created TypeInfos (for pointer types, function types, etc.)
    std::vector<std::unique_ptr<TypeInfo>> dynamicTypes_;

    // Lambda counter for generating unique synthetic function names
    unsigned lambdaCounter_ = 0;

    // Lambda info shared with IRGen: AST node ptr → {funcName, returnType, captures, paramCount}
    struct LambdaInfo {
        std::string funcName;                    // "__lambda_0"
        const TypeInfo* returnType = nullptr;
        std::vector<TypeInfo::CaptureInfo> captures;
        std::vector<const TypeInfo*> paramTypes;
        bool isBlock = false;                    // true for |x| { block }
        LucisParser::BlockContext* blockCtx = nullptr;
        LucisParser::ExpressionContext* exprCtx = nullptr;
    };
    std::unordered_map<const antlr4::ParserRuleContext*, LambdaInfo> lambdaInfo_;

    // ── Type resolution ──────────────────────────────────────────────
    const TypeInfo* resolveTypeSpec(LucisParser::TypeSpecContext* ctx,
                                   unsigned& arrayDims);
    const TypeInfo* resolveExprType(LucisParser::ExpressionContext* expr);
    // Lambda expression type resolution (single-expr body)
    const TypeInfo* resolveLambdaExpr(LucisParser::LambdaExprContext* lexpr);
    // Lambda expression type resolution (block body)
    const TypeInfo* resolveLambdaBlockExpr(LucisParser::LambdaBlockExprContext* lblk);
    const TypeInfo* tryResolveQualifiedType(antlr4::ParserRuleContext* ctx,
                                           const std::string& first,
                                           const std::string& second);
    const TypeInfo* getPointerType(const TypeInfo* pointee);
    const TypeInfo* makeFunctionType(const TypeInfo* returnType,
                                     const std::vector<const TypeInfo*>& paramTypes,
                                     bool isVariadic = false);
    std::string resolveBaseTypeName(LucisParser::TypeSpecContext* ctx);
    const TypeInfo* resolveBuiltinReturnType(const std::string& retName);

    // ── Type queries ─────────────────────────────────────────────────
    bool isNumeric(const TypeInfo* ti);
    bool isInteger(const TypeInfo* ti);
    bool isIntegerOrPointer(const TypeInfo* ti);
    bool isConditionType(const TypeInfo* ti);
    bool isAssignable(const TypeInfo* lhs, const TypeInfo* rhs);
    void checkNegativeToUnsigned(const TypeInfo* target,
                                 LucisParser::ExpressionContext* expr,
                                 antlr4::ParserRuleContext* ctx);
    unsigned resolveExprArrayDims(LucisParser::ExpressionContext* expr);

    // ── Top-level checks ─────────────────────────────────────────────
    void checkUseDecls(LucisParser::ProgramContext* tree);
    void checkTypeAliasDecl(LucisParser::TypeAliasDeclContext* decl);
    void checkStructDecl(LucisParser::StructDeclContext* decl);
    void checkUnionDecl(LucisParser::UnionDeclContext* decl);
    void checkEnumDecl(LucisParser::EnumDeclContext* decl);
    void checkExtendDecl(LucisParser::ExtendDeclContext* decl);
    void checkExtendMethodBodies(LucisParser::ExtendDeclContext* decl);
    void checkExternDecl(LucisParser::ExternDeclContext* decl);
    void registerFunctionSignature(LucisParser::FunctionDeclContext* func);
    void checkFunction(LucisParser::FunctionDeclContext* func);
    bool blockAlwaysReturns(LucisParser::BlockContext* block);
    void registerGlobalBuiltins();

    // ── Statement checks ────────────────────────────────────────────
    void checkBlock(LucisParser::BlockContext* block, const TypeInfo* retType,
                    std::unordered_set<std::string>* initCapture = nullptr);
    void checkStmt(LucisParser::StatementContext* stmt, const TypeInfo* retType, bool& terminated);
    void checkVarDeclStmt(LucisParser::VarDeclStmtContext* stmt);
    void checkConstDeclStmt(LucisParser::ConstDeclStmtContext* stmt);
    bool isValidConstExpr(LucisParser::ExpressionContext* expr);
    void checkAssignStmt(LucisParser::AssignStmtContext* stmt);
    void checkCompoundAssignStmt(LucisParser::CompoundAssignStmtContext* stmt);
    void checkFieldAssignStmt(LucisParser::FieldAssignStmtContext* stmt);
    void checkFieldCompoundAssignStmt(LucisParser::FieldCompoundAssignStmtContext* stmt);
    void checkArrowAssignStmt(LucisParser::ArrowAssignStmtContext* stmt);
    void checkArrowCompoundAssignStmt(LucisParser::ArrowCompoundAssignStmtContext* stmt);
    void checkDerefAssignStmt(LucisParser::DerefAssignStmtContext* stmt);
    void checkDerefCompoundAssignStmt(LucisParser::DerefCompoundAssignStmtContext* stmt);
    void checkCallStmt(LucisParser::CallStmtContext* stmt);
    void checkAsmStmt(LucisParser::AsmStmtContext* stmt);
    void checkExprStmt(LucisParser::ExprStmtContext* stmt);
    void checkReturnStmt(LucisParser::ReturnStmtContext* stmt,
                         const TypeInfo* expectedType);
    void checkIfStmt(LucisParser::IfStmtContext* stmt,
                     const TypeInfo* retType);
    void checkForInStmt(LucisParser::ForInStmtContext* stmt,
                        const TypeInfo* retType);
    void checkForClassicStmt(LucisParser::ForClassicStmtContext* stmt,
                             const TypeInfo* retType);
    void checkSwitchStmt(LucisParser::SwitchStmtContext* stmt,
                         const TypeInfo* retType);

    // ── Flow analysis helpers ───────────────────────────────────────
    bool isTerminatorStmt(LucisParser::StatementContext* stmt);
    void warnUnusedLocals(LucisParser::FunctionDeclContext* func);
    void warnUnusedLocals(antlr4::ParserRuleContext* ctx);

    // ── Error reporting with source location ────────────────────────
    void error(antlr4::ParserRuleContext* ctx, const std::string& msg);
    void warning(antlr4::ParserRuleContext* ctx, const std::string& msg);
    void warningToken(antlr4::Token* start, antlr4::Token* stop,
                      const std::string& msg);
    void errorToken(antlr4::Token* start, antlr4::Token* stop,
                    const std::string& msg);
    void emitDiag(antlr4::Token* start, antlr4::Token* stop,
                  Diagnostic::Severity sev, const std::string& msg);

    // ── Phase 1: FFI buffer safety helpers ──────────────────────────
    std::optional<uint64_t> tryEvalUSizeExpr(LucisParser::ExpressionContext* expr) const;
    std::optional<std::pair<uint64_t, uint64_t>>
        tryEvalUSizeRangeExpr(LucisParser::ExpressionContext* expr) const;
    std::optional<uint64_t> tryGetCStringLiteralLen(LucisParser::ExpressionContext* expr) const;
    VarInfo* resolveTrackedVarFromExpr(LucisParser::ExpressionContext* expr);
    void resetTrackedBufferInfo(VarInfo& vi);
    void resetTrackedNumericInfo(VarInfo& vi);
    void trackVarBufferFromExpr(const std::string& varName,
                                LucisParser::ExpressionContext* expr,
                                const TypeInfo* declaredType);
    void trackVarNumericRangeFromExpr(const std::string& varName,
                                      LucisParser::ExpressionContext* expr,
                                      const TypeInfo* declaredType);
    bool isDropTrackedType(const TypeInfo* type, unsigned arrayDims = 0) const;
    bool exprConsumesOwnership(LucisParser::ExpressionContext* expr) const;
    bool isBorrowedStringExpr(LucisParser::ExpressionContext* expr) const;
    void updateOwnershipOnInitialization(VarInfo& vi, LucisParser::ExpressionContext* expr);
    void markExprAsMoved(LucisParser::ExpressionContext* expr, antlr4::ParserRuleContext* whereCtx);
    void validateExprNotMoved(LucisParser::ExpressionContext* expr, antlr4::ParserRuleContext* whereCtx);
    void applyCallOwnershipEffects(const std::string& calleeName,
                                   const std::vector<LucisParser::ExpressionContext*>& args,
                                   antlr4::ParserRuleContext* whereCtx);
    void analyzeUnsafeCBufferCall(const std::string& funcName,
                                  antlr4::ParserRuleContext* ctx,
                                  const std::vector<LucisParser::ExpressionContext*>& args);

    // ── Module context (set by CLI before check) ────────────────────
    const ModuleRegistry* moduleRegistry_ = nullptr;
    std::string currentModulePath_;
    std::string currentFile_;

    // Project directories for filesystem module fallback (set by LSP).
    std::string projectRoot_;
    std::vector<std::string> sourcePaths_;

    // Try to find a module file on disk using project paths + stdlib dirs.
    // Returns the file path if found, empty otherwise.
    std::string findModuleFile(const std::string& modPath) const;

    // Phase 1: SemanticDB populated in parallel with TypeRegistry
    semantic::SemanticDB* semanticDB_ = nullptr;

    // User imports: symbol name → module path it was imported from
    std::unordered_map<std::string, std::string> userImports_;

    // Injected enum variant names from `use EnumType::*;`
    struct InjectedVariant {
        const TypeInfo* enumType;
        const EnumVariantInfo* variantInfo;
    };
    std::unordered_map<std::string, InjectedVariant> enumVariantImports_;

    // C bindings from parsed #include headers
    const CBindings* cBindings_ = nullptr;

    // C enum constants: name → { type, value }
    struct CEnumConstant {
        const TypeInfo* type;
        int64_t value = 0;
        double  floatValue = 0.0;
        bool    isFloat  = false;
        bool    isString = false;
    };
    std::unordered_map<std::string, CEnumConstant> cEnumConstants_;

    // C global variables: name → type
    std::unordered_map<std::string, const TypeInfo*> cGlobals_;

    // C function-like macros: name → definition
    std::unordered_map<std::string, const CFunctionLikeMacro*> cFunctionLikeMacros_;

    // Check if a function is known (local, same-module, imported, or builtin)
    bool isKnownFunction(const std::string& name) const;
    bool isKnownType(const std::string& name) const;

    // ── User-defined generics (monomorphization) ─────────────────────
    struct GenericStructTemplate {
        std::vector<std::string>      typeParams;  // e.g. {"T"} or {"K", "V"}
        LucisParser::StructDeclContext* decl;        // original AST node (non-owning)
    };
    struct GenericUnionTemplate {
        std::vector<std::string>     typeParams;
        LucisParser::UnionDeclContext* decl;
    };
    struct GenericEnumTemplate {
        std::vector<std::string>    typeParams;
        LucisParser::EnumDeclContext* decl;
    };
    struct GenericFuncTemplate {
        std::vector<std::string>       typeParams;
        LucisParser::FunctionDeclContext* decl;
    };
    struct GenericExtendTemplate {
        std::vector<std::string>       typeParams;
        LucisParser::ExtendDeclContext*  decl;
    };
    std::unordered_map<std::string, GenericStructTemplate>  genericStructTemplates_;
    std::unordered_map<std::string, GenericUnionTemplate>   genericUnionTemplates_;
    std::unordered_map<std::string, GenericEnumTemplate>    genericEnumTemplates_;
    std::unordered_map<std::string, GenericFuncTemplate>    genericFuncTemplates_;
    std::unordered_map<std::string, GenericExtendTemplate>  genericExtendTemplates_;
    // Tracks instantiations in progress for cycle detection
    std::unordered_set<std::string> instantiatingGenerics_;
    // Active type-parameter substitution while checking an instantiated generic body.
    std::unordered_map<std::string, const TypeInfo*> activeTypeSubst_;

    // Mangles "Node" + ["int32"] → "Node__int32"
    static std::string mangleGenericName(const std::string& baseName,
                                         const std::vector<const TypeInfo*>& typeArgs);

    // Produces (and caches) a concrete TypeInfo for a generic struct instantiation.
    // Returns nullptr and emits an error on failure.
    const TypeInfo* instantiateGenericStruct(
        const std::string& baseName,
        const GenericStructTemplate& tmpl,
        const std::vector<const TypeInfo*>& typeArgs,
        antlr4::ParserRuleContext* ctx);

    const TypeInfo* instantiateGenericUnion(
        const std::string& baseName,
        const GenericUnionTemplate& tmpl,
        const std::vector<const TypeInfo*>& typeArgs,
        antlr4::ParserRuleContext* ctx);

    const TypeInfo* instantiateGenericEnum(
        const std::string& baseName,
        const GenericEnumTemplate& tmpl,
        const std::vector<const TypeInfo*>& typeArgs,
        antlr4::ParserRuleContext* ctx);

    // Produces (and caches) a concrete TypeInfo for a generic function instantiation.
    const TypeInfo* instantiateGenericFunc(
        const std::string& baseName,
        const GenericFuncTemplate& tmpl,
        const std::vector<const TypeInfo*>& typeArgs,
        antlr4::ParserRuleContext* ctx);

    // Infers concrete type arguments for a generic call from formal parameters.
    std::optional<std::vector<const TypeInfo*>> inferGenericTypeArgs(
        const std::string& displayName,
        const std::vector<std::string>& typeParams,
        LucisParser::TypeParamListContext* typeParamList,
        const std::vector<LucisParser::ParamContext*>& formalParams,
        const std::vector<const TypeInfo*>& argTypes,
        antlr4::ParserRuleContext* ctx);

    // Unifies a formal type spec against an actual argument type for inference.
    bool unifyGenericTypeArg(
        LucisParser::TypeSpecContext* formalType,
        const TypeInfo* actualType,
        const std::unordered_set<std::string>& genericParams,
        std::unordered_map<std::string, const TypeInfo*>& inferred,
        antlr4::ParserRuleContext* ctx,
        const std::string& displayName,
        bool& emittedSpecificError);

    // Resolves a type spec under a substitution map (typeParam → concrete TypeInfo*).
    const TypeInfo* resolveTypeSpecWithSubst(
        LucisParser::TypeSpecContext* typeSpec,
        const std::unordered_map<std::string, const TypeInfo*>& subst,
        unsigned& arrayDims);

    // Returns the constraint TypeInfo for a type param (e.g. "numeric"), or nullptr.
    const TypeInfo* resolveTypeParamConstraint(const std::string& constraintName,
                                               antlr4::ParserRuleContext* ctx);

    // Checks that a concrete type arg satisfies its constraint.
    bool satisfiesConstraint(const TypeInfo* typeArg,
                             const TypeInfo* constraint,
                             const std::string& paramName,
                             antlr4::ParserRuleContext* ctx);

    // ── Phase 1: SemanticDB parallel population helpers ──────────────

    // Initialize builtins (primitives, extended types) in the SemanticDB.
    void initSemanticDB();

    // Phase 3: verify TypeRegistry and SemanticDB are consistent
    void verifySemanticDBConsistency();

    // Convert old TypeInfo → new semantic::Decl (with lookup in SemanticDB).
    std::unique_ptr<semantic::Decl> typeInfoToDecl(const TypeInfo& ti);
    semantic::SourceLocation toSemanticLoc(antlr4::ParserRuleContext* ctx) const;
    semantic::DeclKind toSemanticKind(TypeKind tk) const;
    semantic::FieldInfo toSemanticField(const ::FieldInfo& f) const;
    semantic::VariantInfo toSemanticVariant(const EnumVariantInfo& v) const;
    semantic::MethodInfo toSemanticMethod(const StructMethodInfo& m) const;

    // Register into SemanticDB in parallel with existing TypeRegistry calls.
    // Called at the end of each check*Decl / registerFunctionSignature.
    void syncToSemanticDB_Struct(const TypeInfo& ti,
                                 const std::string& modulePath,
                                 antlr4::ParserRuleContext* ctx);
    void syncToSemanticDB_Union(const TypeInfo& ti,
                                const std::string& modulePath,
                                antlr4::ParserRuleContext* ctx);
    void syncToSemanticDB_Enum(const TypeInfo& ti,
                               const std::string& modulePath,
                               antlr4::ParserRuleContext* ctx);
    void syncToSemanticDB_TypeAlias(const TypeInfo& ti,
                                    const std::string& modulePath,
                                    antlr4::ParserRuleContext* ctx);
    void syncToSemanticDB_Function(const std::string& name,
                                   const TypeInfo& funcType,
                                   const std::string& modulePath,
                                   antlr4::ParserRuleContext* ctx);
    void syncToSemanticDB_Extend(const std::string& structName,
                                 const std::vector<StructMethodInfo>& methods);
    void syncToSemanticDB_GenericStruct(const std::string& name,
                                        const std::vector<std::string>& typeParams,
                                        LucisParser::StructDeclContext* decl);
    void syncToSemanticDB_GenericUnion(const std::string& name,
                                       const std::vector<std::string>& typeParams,
                                       LucisParser::UnionDeclContext* decl);
    void syncToSemanticDB_GenericEnum(const std::string& name,
                                      const std::vector<std::string>& typeParams,
                                      LucisParser::EnumDeclContext* decl);
    void syncToSemanticDB_GenericFunc(const std::string& name,
                                      const std::vector<std::string>& typeParams,
                                      LucisParser::FunctionDeclContext* decl);
    void syncToSemanticDB_GenericExtend(const std::string& name,
                                        const std::vector<std::string>& typeParams,
                                        LucisParser::ExtendDeclContext* decl);
    void syncToSemanticDB_GenericInstantiation(const std::string& mangledName,
                                               const TypeInfo& concreteTI);
};
