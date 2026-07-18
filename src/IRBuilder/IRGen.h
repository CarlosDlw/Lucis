#pragma once

#include <memory>
#include <string>
#include <any>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DIBuilder.h>

#include "generated/LucisParserBaseVisitor.h"
#include "generated/LucisParser.h"
#include "LLVM_IR/IRModule.h"
#include "imports/ImportResolver.h"
#include "types/TypeRegistry.h"
#include "types/MethodRegistry.h"
#include "types/ExtendedTypeRegistry.h"
#include "intrinsics/IntrinsicRegistry.h"
#include "comptime/ComptimeEngine.h"
#include "ffi/ABIInfo.h"
#include "semantic/SemanticDB.h"
#include "ffi/CBindings.h"
class ModuleRegistry;
class CBindings;
struct CStructMacro;

class IRGen : public LucisParserBaseVisitor {
public:
    IRGen();

    // Walk the parse tree and produce an IRModule.
    std::unique_ptr<IRModule> generate(LucisParser::ProgramContext* tree,
                                       const std::string& moduleName);

    // Set module context for cross-file symbol resolution and mangling.
    void setModuleContext(const ModuleRegistry* registry,
                          const std::string& modulePath,
                          const std::string& currentFile,
                          bool isStdlib = false);

    // Set C bindings from parsed #include headers.
    void setCBindings(const CBindings* bindings);

    // Phase 2: set SemanticDB for authoritative type data (avoids re-walking AST)
    void setSemanticDB(const semantic::SemanticDB* db) { semanticDB_ = db; }

    // Set project root for output paths (inline assembly, etc.)
    void setProjectRoot(const std::string& root) { projectRoot_ = root; }

    // Set custom target triple (freestanding/cross-compilation). Empty = use host default.
    void setTargetTriple(const std::string& triple) { targetTriple_ = triple; }

    // Disable stdlib/libruntime dependencies (kernel/freestanding)
    void setNoStd(bool v) { noStd_ = v; }

    // Set comptime engine for compile-time function evaluation
    void setComptimeEngine(ComptimeEngine* engine) { comptimeEngine_ = engine; }

    // Enable/disable DWARF debug info emission
    void setEmitDebugInfo(bool v) { emitDebugInfo_ = v; }

    // Set debug output mode (--debug flag) for debug_assertions / @cfg("debug")
    void setDebugOutput(bool v) { debugOutput_ = v; }

    // ── Visitor overrides ───────────────────────────────────────────────────
    std::any visitProgram(LucisParser::ProgramContext* ctx)             override;
    std::any visitStructDecl(LucisParser::StructDeclContext* ctx)       override;
    std::any visitUnionDecl(LucisParser::UnionDeclContext* ctx)         override;
    std::any visitEnumDecl(LucisParser::EnumDeclContext* ctx)           override;
    std::any visitExternDecl(LucisParser::ExternDeclContext* ctx)       override;
    std::any visitFunctionDecl(LucisParser::FunctionDeclContext* ctx)   override;
    std::any visitBlock(LucisParser::BlockContext* ctx)                 override;
    std::any visitUseRoot(LucisParser::UseRootContext* ctx)             override;
    std::any visitUseItem(LucisParser::UseItemContext* ctx)             override;
    std::any visitUseGroup(LucisParser::UseGroupContext* ctx)           override;
    std::any visitUseEnumWildcard(LucisParser::UseEnumWildcardContext* ctx) override;
    std::any visitConstDeclStmt(LucisParser::ConstDeclStmtContext* ctx) override;
    std::any visitVarDeclStmt(LucisParser::VarDeclStmtContext* ctx)     override;
    std::any visitAssignStmt(LucisParser::AssignStmtContext* ctx)       override;
    std::any visitCompoundAssignStmt(LucisParser::CompoundAssignStmtContext* ctx) override;
    std::any visitFieldAssignStmt(LucisParser::FieldAssignStmtContext* ctx) override;
    std::any visitFieldCompoundAssignStmt(LucisParser::FieldCompoundAssignStmtContext* ctx) override;
    std::any visitIndexFieldAssignStmt(LucisParser::IndexFieldAssignStmtContext* ctx) override;
    std::any visitFieldIndexAssignStmt(LucisParser::FieldIndexAssignStmtContext* ctx) override;
    std::any visitArrowAssignStmt(LucisParser::ArrowAssignStmtContext* ctx) override;
    std::any visitArrowCompoundAssignStmt(LucisParser::ArrowCompoundAssignStmtContext* ctx) override;
    std::any visitArrowAnyAssignStmt(LucisParser::ArrowAnyAssignStmtContext* ctx) override;
    std::any visitArrowAnyCompoundAssignStmt(LucisParser::ArrowAnyCompoundAssignStmtContext* ctx) override;
    std::any visitCallStmt(LucisParser::CallStmtContext* ctx)           override;
    std::any visitAsmStmt(LucisParser::AsmStmtContext* ctx)             override;
    std::any visitLabelDef(LucisParser::LabelDefContext* ctx)           override;
    std::any visitExprStmt(LucisParser::ExprStmtContext* ctx)           override;
    std::any visitReturnStmt(LucisParser::ReturnStmtContext* ctx)       override;
    std::any visitIfStmt(LucisParser::IfStmtContext* ctx)               override;
    std::any visitForInStmt(LucisParser::ForInStmtContext* ctx)         override;
    std::any visitForClassicStmt(LucisParser::ForClassicStmtContext* ctx) override;
    std::any visitBreakStmt(LucisParser::BreakStmtContext* ctx)         override;
    std::any visitContinueStmt(LucisParser::ContinueStmtContext* ctx)   override;
    std::any visitLoopStmt(LucisParser::LoopStmtContext* ctx)           override;
    std::any visitWhileStmt(LucisParser::WhileStmtContext* ctx)         override;
    std::any visitDoWhileStmt(LucisParser::DoWhileStmtContext* ctx)     override;
    std::any visitSwitchStmt(LucisParser::SwitchStmtContext* ctx)       override;
    std::any visitMethodCallExpr(LucisParser::MethodCallExprContext* ctx) override;
    std::any visitArrowMethodCallExpr(LucisParser::ArrowMethodCallExprContext* ctx) override;
    std::any visitTypeSpec(LucisParser::TypeSpecContext* ctx)           override;

    // Expression visitors (labeled alternatives)
    std::any visitIntLitExpr(LucisParser::IntLitExprContext* ctx)       override;
    std::any visitHexLitExpr(LucisParser::HexLitExprContext* ctx)       override;
    std::any visitOctLitExpr(LucisParser::OctLitExprContext* ctx)       override;
    std::any visitBinLitExpr(LucisParser::BinLitExprContext* ctx)       override;
    std::any visitSuffixedIntLitExpr(LucisParser::SuffixedIntLitExprContext* ctx) override;
    std::any visitSuffixedIntFloatExpr(LucisParser::SuffixedIntFloatExprContext* ctx) override;
    std::any visitSuffixedHexLitExpr(LucisParser::SuffixedHexLitExprContext* ctx) override;
    std::any visitSuffixedOctLitExpr(LucisParser::SuffixedOctLitExprContext* ctx) override;
    std::any visitSuffixedBinLitExpr(LucisParser::SuffixedBinLitExprContext* ctx) override;
    std::any visitSuffixedFloatLitExpr(LucisParser::SuffixedFloatLitExprContext* ctx) override;
    std::any visitSuffixedFloatIntExpr(LucisParser::SuffixedFloatIntExprContext* ctx) override;
    std::any visitSuffixedLeadingDotFloatExpr(LucisParser::SuffixedLeadingDotFloatExprContext* ctx) override;
    std::any visitFloatLitExpr(LucisParser::FloatLitExprContext* ctx)   override;
    std::any visitLeadingDotFloatLitExpr(LucisParser::LeadingDotFloatLitExprContext* ctx) override;
    std::any visitBoolLitExpr(LucisParser::BoolLitExprContext* ctx)     override;
    std::any visitCharLitExpr(LucisParser::CharLitExprContext* ctx)     override;
    std::any visitStrLitExpr(LucisParser::StrLitExprContext* ctx)       override;
    std::any visitCStrLitExpr(LucisParser::CStrLitExprContext* ctx)   override;
    std::any visitBtickExpr(LucisParser::BtickExprContext* ctx)         override;
    std::any visitRawBtickExpr(LucisParser::RawBtickExprContext* ctx)     override;
    std::any visitIntBtickExpr(LucisParser::IntBtickExprContext* ctx)     override;
    std::any visitShellBtickExpr(LucisParser::ShellBtickExprContext* ctx) override;
    std::any visitCmptBtickExpr(LucisParser::CmptBtickExprContext* ctx)   override;
    std::any visitAsmExpr(LucisParser::AsmExprContext* ctx)           override;
    std::any visitIdentExpr(LucisParser::IdentExprContext* ctx)         override;
    std::any visitCfgExpr(LucisParser::CfgExprContext* ctx)             override;
    std::any visitAtPtrExpr(LucisParser::AtPtrExprContext* ctx)          override;

    // ── @cfg(...) compile-time evaluation helpers ─────────────
    bool evalCfgPredicate(LucisParser::ExpressionContext* expr);
    std::optional<std::string> evalCfgIdentOrString(LucisParser::ExpressionContext* expr);
    std::optional<int64_t> evalCfgIntValue(LucisParser::ExpressionContext* expr);
    std::optional<bool> getCfgTargetValue(const std::string& name);
    std::optional<std::string> getCfgTargetString(const std::string& name);
    std::optional<int64_t> getCfgTargetInt(const std::string& name);
    std::any visitArrayLitExpr(LucisParser::ArrayLitExprContext* ctx)   override;
    std::any visitListCompExpr(LucisParser::ListCompExprContext* ctx)   override;
    std::any visitIndexExpr(LucisParser::IndexExprContext* ctx)         override;
    std::any visitStructLitExpr(LucisParser::StructLitExprContext* ctx) override;
    std::any visitStructPosInitExpr(LucisParser::StructPosInitExprContext* ctx) override;
    std::any visitFieldAccessExpr(LucisParser::FieldAccessExprContext* ctx) override;
    std::any visitArrowAccessExpr(LucisParser::ArrowAccessExprContext* ctx) override;
    std::any visitEnumAccessExpr(LucisParser::EnumAccessExprContext* ctx) override;
    std::any visitGenericEnumAccessExpr(LucisParser::GenericEnumAccessExprContext* ctx) override;
    std::any visitQualifiedStructPosInitExpr(LucisParser::QualifiedStructPosInitExprContext* ctx) override;
    std::any visitQualifiedStructNamedInitExpr(LucisParser::QualifiedStructNamedInitExprContext* ctx) override;
    std::any visitGenericEnumNamedVariantExpr(LucisParser::GenericEnumNamedVariantExprContext* ctx) override;
    std::any visitGenericEnumPosVariantExpr(LucisParser::GenericEnumPosVariantExprContext* ctx) override;
    std::any visitStaticMethodCallExpr(LucisParser::StaticMethodCallExprContext* ctx) override;
    std::any visitNullLitExpr(LucisParser::NullLitExprContext* ctx)     override;
    std::any visitAddrOfExpr(LucisParser::AddrOfExprContext* ctx)       override;
    std::any visitDerefExpr(LucisParser::DerefExprContext* ctx)         override;
    std::any visitDerefAssignStmt(LucisParser::DerefAssignStmtContext* ctx) override;
    std::any visitDerefCompoundAssignStmt(LucisParser::DerefCompoundAssignStmtContext* ctx) override;
    std::any visitTypeAliasDecl(LucisParser::TypeAliasDeclContext* ctx) override;
    std::any visitFnCallExpr(LucisParser::FnCallExprContext* ctx)       override;
    std::any visitNegExpr(LucisParser::NegExprContext* ctx)             override;
    std::any visitMulExpr(LucisParser::MulExprContext* ctx)             override;
    std::any visitAddSubExpr(LucisParser::AddSubExprContext* ctx)       override;
    std::any visitRelExpr(LucisParser::RelExprContext* ctx)             override;
    std::any visitEqExpr(LucisParser::EqExprContext* ctx)               override;
    std::any visitLogicalNotExpr(LucisParser::LogicalNotExprContext* ctx) override;
    std::any visitLogicalAndExpr(LucisParser::LogicalAndExprContext* ctx) override;
    std::any visitLogicalOrExpr(LucisParser::LogicalOrExprContext* ctx) override;
    std::any visitBitNotExpr(LucisParser::BitNotExprContext* ctx)       override;
    std::any visitLshiftExpr(LucisParser::LshiftExprContext* ctx)       override;
    std::any visitRshiftExpr(LucisParser::RshiftExprContext* ctx)       override;
    std::any visitBitAndExpr(LucisParser::BitAndExprContext* ctx)       override;
    std::any visitBitXorExpr(LucisParser::BitXorExprContext* ctx)       override;
    std::any visitBitOrExpr(LucisParser::BitOrExprContext* ctx)         override;
    std::pair<llvm::Value*, llvm::Type*>
        resolveIncrDecrTarget(LucisParser::ExpressionContext* expr);
    llvm::Value* resolveMethodReceiverAddress(LucisParser::ExpressionContext* expr);
    std::any visitPreIncrExpr(LucisParser::PreIncrExprContext* ctx)     override;
    std::any visitPreDecrExpr(LucisParser::PreDecrExprContext* ctx)     override;
    std::any visitPostIncrExpr(LucisParser::PostIncrExprContext* ctx)   override;
    std::any visitPostDecrExpr(LucisParser::PostDecrExprContext* ctx)   override;
    std::any visitCastExpr(LucisParser::CastExprContext* ctx)           override;
    std::any visitSizeofExpr(LucisParser::SizeofExprContext* ctx)       override;
    std::any visitTypeofExpr(LucisParser::TypeofExprContext* ctx)      override;

    std::any visitAlignofExpr(LucisParser::AlignofExprContext* ctx)      override;
    std::any visitOffsetofExpr(LucisParser::OffsetofExprContext* ctx)    override;
    std::any visitTernaryExpr(LucisParser::TernaryExprContext* ctx)     override;
    std::any visitIsExpr(LucisParser::IsExprContext* ctx)               override;
    std::any visitNullCoalExpr(LucisParser::NullCoalExprContext* ctx)   override;
    std::any visitRangeExpr(LucisParser::RangeExprContext* ctx)         override;
    std::any visitRangeInclExpr(LucisParser::RangeInclExprContext* ctx) override;
    std::any visitSpreadExpr(LucisParser::SpreadExprContext* ctx)       override;
    std::any visitParenExpr(LucisParser::ParenExprContext* ctx)         override;
    std::any visitTupleLitExpr(LucisParser::TupleLitExprContext* ctx)   override;
    std::any visitTupleIndexExpr(LucisParser::TupleIndexExprContext* ctx) override;
    std::any visitChainedTupleIndexExpr(LucisParser::ChainedTupleIndexExprContext* ctx) override;
    std::any visitTupleArrowIndexExpr(LucisParser::TupleArrowIndexExprContext* ctx) override;
    std::any visitChainedTupleArrowIndexExpr(LucisParser::ChainedTupleArrowIndexExprContext* ctx) override;
    std::any visitSpawnExpr(LucisParser::SpawnExprContext* ctx)         override;
    std::any visitAwaitExpr(LucisParser::AwaitExprContext* ctx)         override;
    std::any visitLockStmt(LucisParser::LockStmtContext* ctx)           override;
    std::any visitTryCatchStmt(LucisParser::TryCatchStmtContext* ctx)   override;
    std::any visitThrowStmt(LucisParser::ThrowStmtContext* ctx)         override;
    std::any visitTryExpr(LucisParser::TryExprContext* ctx)             override;
    std::any visitCatchUnwrapExpr(LucisParser::CatchUnwrapExprContext* ctx) override;
    std::any visitPropagateExpr(LucisParser::PropagateExprContext* ctx) override;
    std::any visitLambdaExpr(LucisParser::LambdaExprContext* ctx)       override;
    std::any visitLambdaBlockExpr(LucisParser::LambdaBlockExprContext* ctx) override;
    std::any visitMatchExpr(LucisParser::MatchExprContext* ctx)         override;
    std::any visitExtendDecl(LucisParser::ExtendDeclContext* ctx)        override;
    std::any visitDeferStmt(LucisParser::DeferStmtContext* ctx)           override;
    std::any visitNakedBlockStmt(LucisParser::NakedBlockStmtContext* ctx) override;
    std::any visitInlineBlockStmt(LucisParser::InlineBlockStmtContext* ctx) override;
    std::any visitScopeBlockStmt(LucisParser::ScopeBlockStmtContext* ctx) override;
    std::any visitCMacroBlock(LucisParser::CMacroBlockContext* ctx) override;
    std::any visitAsmBBlock(LucisParser::AsmBBlockContext* ctx) override;
    // Generic expression visitors
    std::any visitGenericFnCallExpr(LucisParser::GenericFnCallExprContext* ctx) override;
    std::any visitGenericStaticMethodCallExpr(LucisParser::GenericStaticMethodCallExprContext* ctx) override;
    std::any visitGenericQualifiedFnCallExpr(LucisParser::GenericQualifiedFnCallExprContext* ctx) override;
    std::any visitGenericStructLitExpr(LucisParser::GenericStructLitExprContext* ctx) override;
    std::any visitGenericStructPosInitExpr(LucisParser::GenericStructPosInitExprContext* ctx) override;

private:
    // Non-owning pointers valid only during generate().
    llvm::LLVMContext*  context_         = nullptr;
    llvm::Module*       module_          = nullptr;
    llvm::IRBuilder<>*  builder_         = nullptr;
    llvm::Function*     currentFunction_ = nullptr;

    // Symbol table: variable name → {alloca, type info}
    struct VarInfo {
        llvm::AllocaInst* alloca;
        const TypeInfo*   typeInfo;
        unsigned          arrayDims = 0; // 0 = scalar, 1 = []T, 2 = [][]T, etc.
        bool              isParam   = false; // true = borrowed from caller, skip auto-free
        bool              isBorrowed = false; // true = borrowed string view, skip auto-free
        bool              consumed  = false; // true = ownership transferred, skip auto-free
        bool              ownsContainers = true; // false = Vec/Map/Set fields are shallow copies, skip free
        std::vector<unsigned> fixedArraySizes; // declared [N] sizes (incl. pointer-to-array)
    };
    std::unordered_map<std::string, VarInfo> locals_;

    // Top-level consts: name → {LLVM global, TypeInfo}
    struct TopLevelConst {
        llvm::GlobalVariable* global;
        const TypeInfo*       typeInfo;
        unsigned              arrayDims = 0;
    };
    std::unordered_map<std::string, TopLevelConst> topLevelConsts_;

    // Const declarators pending runtime initialization: (decl, global)
    std::vector<std::pair<LucisParser::ConstDeclaratorContext*, llvm::GlobalVariable*>> pendingConstDecls_;

    // Labels declared in the current function, mapped to their BasicBlock
    std::unordered_map<std::string, llvm::BasicBlock*> labels_;

    // Variadic parameter info (name → {data ptr alloca, len alloca, element type})
    struct VariadicParamInfo {
        llvm::AllocaInst* dataPtr;      // alloca storing pointer to the data array
        llvm::AllocaInst* lenAlloca;    // alloca storing the element count (i64)
        const TypeInfo*   elementType;  // type of each element
    };
    std::unordered_map<std::string, VariadicParamInfo> variadicParams_;

    // Tracks which functions are variadic and the index of their variadic param
    struct VariadicFuncInfo {
        size_t variadicParamIdx;    // index in the grammar param list
        const TypeInfo* elementType; // type of each variadic element
    };
    std::unordered_map<std::string, VariadicFuncInfo> variadicFunctions_;

    // Import resolver for `use` declarations
    ImportResolver imports_;

    // Global builtins (always available, no `use` required)
    std::unordered_set<std::string> globalBuiltins_;

    // Extern C functions declared via FFI `extern` keyword
    std::unordered_set<std::string> externCFunctions_;

    // Centralized type registry
    TypeRegistry typeRegistry_;

    // Built-in type method registry
    MethodRegistry methodRegistry_;

    // Extended type registry (vec, map, etc.)
    ExtendedTypeRegistry extTypeRegistry_;

    // Intrinsic functions (lucis::core::trap, etc.)
    IntrinsicRegistry intrinsicRegistry_;

    // Loop break/continue target stack
    struct LoopInfo {
        llvm::BasicBlock* breakTarget;
        llvm::BasicBlock* continueTarget;
    };
    std::vector<LoopInfo> loopStack_;
    std::vector<std::unordered_map<std::string, VarInfo>> loopBodyLocalsStack_;

    // Deferred statements (function-scoped, LIFO execution order)
    struct DeferredStmt {
        LucisParser::CallStmtContext* callCtx = nullptr;
        LucisParser::ExprStmtContext* exprCtx = nullptr;
        LucisParser::ScopeCallbackContext* scopeCbCtx = nullptr;
    };
    std::vector<DeferredStmt> deferStack_;

    // Spawn counter for unique wrapper function names
    unsigned spawnCounter_ = 0;

    // Lambda counter for unique synthetic function names
    unsigned lambdaCounter_ = 0;
    // Cache generated lambda functions per AST node (avoid double generation)
    std::unordered_map<const antlr4::ParserRuleContext*, llvm::Function*> lambdaCache_;

    struct MethodEntry {
        llvm::Function* fn;
        const TypeInfo* returnType;
    };

    // Struct methods registered via `extend` blocks
    // structName → { methodName → MethodEntry }
    std::unordered_map<std::string,
        std::unordered_map<std::string, MethodEntry>> structMethods_;

    // Static struct methods (no &self)
    std::unordered_map<std::string,
        std::unordered_map<std::string, MethodEntry>> staticStructMethods_;

    // Operator overloads registered via `extend` blocks
    // structName → { internalName → MethodEntry }
    std::unordered_map<std::string,
        std::unordered_map<std::string, MethodEntry>> operatorMethods_;

    // walk parent chain for method lookup
    const TypeInfo* findMethodReturnInChain(const TypeInfo* ti,
                                             const std::string& name) const;
    MethodEntry* findMethodEntryInChain(const TypeInfo* ti,
                                          const std::string& name);
    // walk parent chain for operator lookup
    MethodEntry* findOperatorEntryInChain(const TypeInfo* ti,
                                           const std::string& internalName);

    // Module context for cross-file resolution
    const ModuleRegistry* moduleRegistry_ = nullptr;
    std::string currentModulePath_;
    std::string currentFile_;
    bool isStdlib_ = false;

    // Phase 2: authoritative semantic database (populated by Checker)
    const semantic::SemanticDB* semanticDB_ = nullptr;

    // Maps function name → LLVM function name (for call resolution)
    std::unordered_map<std::string, std::string> callTargetMap_;

    // User imports: symbol name → source module path
    std::unordered_map<std::string, std::string> userImports_;

    // Caches resolved TypeInfo for expression nodes (used by resolveExprTypeInfo)
    std::unordered_map<const antlr4::ParserRuleContext*, const TypeInfo*> exprTypeCache_;

    // Injected enum variant names from `use EnumType::*;`
    struct InjectedEnumVariant {
        const TypeInfo* enumType;
        const EnumVariantInfo* variantInfo;
    };
    std::unordered_map<std::string, InjectedEnumVariant> enumVariantImports_;

    // C bindings from parsed #include headers
    const CBindings* cBindings_ = nullptr;

    // C bindings from c_macro { ... } blocks (owned by IRGen)
    CBindings cMacroBindings_;

    // Debug info (DIBuilder)
    llvm::DIBuilder* dbgBuilder_ = nullptr;
    llvm::DICompileUnit* dbgCU_ = nullptr;
    llvm::DIFile* dbgFile_ = nullptr;
    llvm::DISubprogram* currentDbgScope_ = nullptr;
    bool emitDebugInfo_ = false;

    // Debug mode flag (--debug): controls debug_assertions / @cfg("debug")
    bool debugOutput_ = false;

    // Comptime engine for compile-time function evaluation
    ComptimeEngine* comptimeEngine_ = nullptr;

    // Project root for inline assembly output paths
    std::string projectRoot_;

    // Custom target triple (empty = use host default)
    std::string targetTriple_;
    bool noStd_ = false;

    // C enum constants: qualified name → integer value
    std::unordered_map<std::string, int64_t> cEnumConstants_;

    // C float constants: name → double value
    std::unordered_map<std::string, double> cFloatConstants_;

    // C string constants: name → string value
    std::unordered_map<std::string, std::string> cStringConstants_;

    // Compile-time known integer values (for consts and locals initialized with literals)
    std::unordered_map<std::string, int64_t> compileTimeValues_;

    // C struct literal macros: name → CStructMacro pointer (owned by CBindings)
    std::unordered_map<std::string, const CStructMacro*> cStructMacros_;

    // C global variables: name → { LLVM global, TypeInfo }
    struct CGlobalEntry {
        llvm::GlobalVariable* global;
        const TypeInfo* type;
    };
    std::unordered_map<std::string, CGlobalEntry> cGlobals_;

    // ABI coercion info for C functions that pass/return structs by value
    std::unordered_map<std::string, CABIInfo> cabiInfos_;

    // Function return TypeInfo cache (for tuple and complex return types)
    std::unordered_map<std::string, const TypeInfo*> fnReturnTypes_;
    std::unordered_map<std::string, unsigned> fnReturnArrayDims_;

    // Current function's return type (for ? propagation between compatible enums)
    const TypeInfo* currentFnReturnType_ = nullptr;

    // Pending payload binding from `expr is EnumType::Variant(name)` expressions.
    // Set by visitIsExpr when a binding identifier is present; consumed by
    // visitIfStmt before emitting the true-branch body so the binding is
    // accessible as a local variable inside that block.
    struct PendingIsBinding {
        std::string     name;       // binding variable name
        llvm::Value*    payloadPtr; // bitcast pointer to the payload field
        const TypeInfo* typeInfo;   // semantic type of the payload
        llvm::Type*     llvmType;   // LLVM type of the payload
    };
    std::optional<PendingIsBinding> pendingIsBinding_;

    // Lazily declare a C function from CBindings when first called.
    llvm::Function* declareCFunction(const std::string& name);

    // Pre-processing helpers for cross-file symbols
    void registerCrossFileSymbols(LucisParser::ProgramContext* ctx);
    void declareExternFunction(const std::string& mangledName,
                               LucisParser::FunctionDeclContext* decl);

    // Forward-declare a user function (signature only, no body)
    void forwardDeclareFunction(LucisParser::FunctionDeclContext* decl);
    
    // Generate initialization function for top-level consts
    void generateConstInitFunction();
    
    std::string resolveCallTarget(const std::string& name) const;

    // Recursively coerce scalars/aggregates to a target LLVM type.
    llvm::Value* coerceValueToType(llvm::Value* value,
                                   llvm::Type* targetType,
                                   bool assumeSignedInt = true);
    llvm::Value* ptrToIntIfNeeded(llvm::Value* val);

    // ── User-defined generics ──────────────────────────────────────────────
    // Generic struct and function template registries
    struct GenericStructTemplate {
        std::vector<std::string>      typeParams;
        LucisParser::StructDeclContext* decl;
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
        std::vector<std::string>        typeParams;
        LucisParser::FunctionDeclContext* decl;
    };
    struct GenericExtendTemplate {
        std::vector<std::string>      typeParams;
        LucisParser::ExtendDeclContext*  decl;
    };
    std::unordered_map<std::string, GenericStructTemplate>  genericStructTemplates_;
    std::unordered_map<std::string, GenericUnionTemplate>   genericUnionTemplates_;
    std::unordered_map<std::string, GenericEnumTemplate>    genericEnumTemplates_;
    std::unordered_map<std::string, GenericFuncTemplate>    genericFuncTemplates_;
    std::unordered_map<std::string, GenericExtendTemplate>  genericExtendTemplates_;
    // Prevents re-instantiation (also detects cycles)
    std::unordered_set<std::string> instantiatedGenerics_;
    // Active type-param substitution during generic body emission (T → int32, etc.)
    std::unordered_map<std::string, const TypeInfo*> currentGenericSubst_;

    // Mangles "Node" + ["int32"] → "Node__int32"
    static std::string mangleGenericName(const std::string& baseName,
                                          const std::vector<const TypeInfo*>& typeArgs);

    // Resolves a type spec with a substitution map (type param → concrete TypeInfo*)
    const TypeInfo* resolveTypeInfoWithSubst(
        LucisParser::TypeSpecContext* ctx,
        const std::unordered_map<std::string, const TypeInfo*>& subst);

    // Ensures the LLVM struct type for a generic struct instance exists.
    // Returns the LLVM StructType* on success, nullptr on error.
    llvm::StructType* ensureGenericStructType(const std::string& mangledName,
                                               const TypeInfo* instanceTI);

    // Instantiates a generic struct (TypeInfo + LLVM struct type).
    // Returns the mangled TypeInfo* (or nullptr on error).
    const TypeInfo* instantiateGenericStruct(
        const std::string& baseName,
        const GenericStructTemplate& tmpl,
        const std::vector<const TypeInfo*>& typeArgs);

    const TypeInfo* instantiateGenericUnion(
        const std::string& baseName,
        const GenericUnionTemplate& tmpl,
        const std::vector<const TypeInfo*>& typeArgs);

    const TypeInfo* instantiateGenericEnum(
        const std::string& baseName,
        const GenericEnumTemplate& tmpl,
        const std::vector<const TypeInfo*>& typeArgs);

    // Instantiates a generic function (declares LLVM function, emits body).
    // Returns the LLVM Function* (or nullptr on error).
    llvm::Function* instantiateGenericFunc(
        const std::string& baseName,
        const GenericFuncTemplate& tmpl,
        const std::vector<const TypeInfo*>& typeArgs);

    std::optional<std::vector<const TypeInfo*>> inferGenericTypeArgs(
        const std::vector<std::string>& typeParams,
        const std::vector<LucisParser::ParamContext*>& formalParams,
        const std::vector<const TypeInfo*>& argTypes);

    bool unifyGenericTypeArg(
        LucisParser::TypeSpecContext* formalType,
        const TypeInfo* actualType,
        const std::unordered_set<std::string>& genericParams,
        std::unordered_map<std::string, const TypeInfo*>& inferred);

    // Helpers
    const TypeInfo*    resolveTypeInfo(LucisParser::TypeSpecContext* ctx);
    // Attribute helpers: check if an attribute list contains a named attribute
    static bool hasAttribute(LucisParser::AttributeListContext* attrs, const std::string& name);
    // Get a string argument from an attribute (e.g., link_section("mysect"))
    static std::string getAttributeStringArg(LucisParser::AttributeListContext* attrs, const std::string& name);

    // Phase 2: Sync a type from SemanticDB into the local TypeRegistry.
    // Returns true if the type was successfully registered (or already exists).
    bool syncTypeFromSemanticDB(const std::string& name);
    const TypeInfo*    resolveExprTypeInfo(LucisParser::ExpressionContext* ctx);
    unsigned           resolveExprArrayDims(LucisParser::ExpressionContext* ctx);
    bool               isPointerType(LucisParser::TypeSpecContext* ctx);
    unsigned           countArrayDims(LucisParser::TypeSpecContext* ctx);
    llvm::ArrayType*   buildTargetArrayType(llvm::Type* srcTy, llvm::Type* elemTy);
    void               storeArrayElements(llvm::Value* src, llvm::Value* destPtr,
                                          llvm::Type* destArrTy, const TypeInfo* elemTI,
                                          unsigned dims);
    llvm::FunctionCallee declareBuiltin(const std::string& name,
                                        llvm::Type* retType,
                                        llvm::ArrayRef<llvm::Type*> argTypes);
    llvm::Type*         getEnumVariantPayloadType(const EnumVariantInfo& variantInfo);
    llvm::Type*         buildFieldLLVMType(const TypeInfo* elemTI, unsigned arrayDims,
                                          const std::vector<unsigned>& arraySizes);
    struct ArrowLValue {
        llvm::Value* ptr = nullptr;
        llvm::Type*  ty = nullptr;
        const TypeInfo* fieldTI = nullptr;
    };
    ArrowLValue         resolveArrowLValue(
        const std::vector<antlr4::tree::TerminalNode*>& identifiers,
        const std::vector<antlr4::tree::TerminalNode*>& dotIdentifiers,
        size_t numArrows, size_t numBrackets,
        const std::vector<LucisParser::ExpressionContext*>& indexExprs);
    ArrowLValue         resolveArrowAnyLValue(
        const std::vector<antlr4::tree::TerminalNode*>& identifiers,
        const std::vector<antlr4::tree::TerminalNode*>& dots,
        const std::vector<antlr4::tree::TerminalNode*>& arrows,
        const std::vector<LucisParser::ExpressionContext*>& indexExprs);
    llvm::Value*        buildEnumVariantValue(const TypeInfo* enumType,
                                             const EnumVariantInfo& variantInfo,
                                             const std::vector<llvm::Value*>& payloadValues);
    llvm::StructType*   getOrCreateVecStructType();
    llvm::StructType*   getOrCreateMapStructType();
    llvm::StructType*   getOrCreateSetStructType();
    std::string         getVecSuffix(const TypeInfo* elemTI);
    void                emitDeferredCleanups();
    void                emitOneDeferred(const DeferredStmt& ds);
    void                emitAutoCleanups(const std::string& skipVar = "");
    void                emitAllCleanups(const std::string& skipVar = "");
    void                emitCleanupForLocal(const std::string& name, const VarInfo& info);
    llvm::Value*        buildVecValueFromArrayLiteral(
                            LucisParser::ArrayLitExprContext* arrLit,
                            const TypeInfo* vecType,
                            const std::string& tempNameHint = "vec_lit");
    void                emitBlockExitCleanups(const std::unordered_map<std::string, VarInfo>& savedLocals);
    bool                isDropTrackedLocal(const VarInfo& info) const;
    bool                isBorrowedStringValueExpr(LucisParser::ExpressionContext* expr) const;
    void                consumeLocalByName(const std::string& name);
    void                consumeExprIfOwnedLocal(LucisParser::ExpressionContext* expr);
    llvm::Value*        deepCopyOwnedFields(llvm::Value* val, llvm::Type* ty);
    void                emitScopeCallback(LucisParser::ScopeCallbackContext* ctx);
    void                emitDivByZeroGuard(llvm::Value* divisor,
                                           antlr4::Token* opToken);
    void                emitBoundsCheck(llvm::Value* index,
                                        llvm::Value* length,
                                        const std::string& name);
    bool                requireArgs(const std::string& funcName,
                                    const std::vector<llvm::Value*>& args,
                                    size_t expected);
    llvm::Value*        castValue(std::any result);
    llvm::Value*        toBool(llvm::Value* value,
                                const TypeInfo* valueTI = nullptr,
                                const std::string& name = "tobool");
    std::pair<llvm::Value*, llvm::Value*>
                       promoteArithmetic(llvm::Value* lhs, llvm::Value* rhs);
    // Lower [N]T array value to {T*, i64} slice for []T fields/params
    llvm::Value*       lowerArrayToSlice(llvm::Value* arrVal, llvm::Type* sliceTy);

    // ── Debug info helpers ─────────────────────────────────────────────────
    // Map a Lucis TypeInfo to an llvm::DIType for DWARF debug info.
    llvm::DIType* getOrCreateDIType(const TypeInfo* ti);
    // Emit dbg.declare for an alloca'd variable.
    void emitDbgDeclare(llvm::AllocaInst* alloca, const std::string& name,
                        const TypeInfo* ti, unsigned line, unsigned argNo = 0);
    // Cache of created DITypes to avoid infinite recursion (e.g. *Node → Node → field *Node → ...)
    std::unordered_map<std::string, llvm::DIType*> createdDITypes_;

    // ── Inline assembly blocks ──────────────────────────────────────────────
public:
    struct InlineAsmFile {
        std::string filePath;
    };
    const std::vector<InlineAsmFile>& inlineAssemblyFiles() const { return inlineAssemblyFiles_; }

private:
    std::vector<InlineAsmFile> inlineAssemblyFiles_;
};
