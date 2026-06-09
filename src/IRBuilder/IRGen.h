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

#include "generated/LuxParserBaseVisitor.h"
#include "generated/LuxParser.h"
#include "LLVM_IR/IRModule.h"
#include "imports/ImportResolver.h"
#include "types/TypeRegistry.h"
#include "types/MethodRegistry.h"
#include "types/ExtendedTypeRegistry.h"
#include "intrinsics/IntrinsicRegistry.h"
#include "ffi/ABIInfo.h"

class NamespaceRegistry;
class CBindings;
struct CStructMacro;

class IRGen : public LuxParserBaseVisitor {
public:
    IRGen();

    // Walk the parse tree and produce an IRModule.
    std::unique_ptr<IRModule> generate(LuxParser::ProgramContext* tree,
                                       const std::string& moduleName);

    // Set namespace context for cross-file symbol resolution and mangling.
    void setNamespaceContext(const NamespaceRegistry* registry,
                             const std::string& currentNamespace,
                             const std::string& currentFile);

    // Set C bindings from parsed #include headers.
    void setCBindings(const CBindings* bindings);

    // ── Visitor overrides ───────────────────────────────────────────────────
    std::any visitProgram(LuxParser::ProgramContext* ctx)             override;
    std::any visitStructDecl(LuxParser::StructDeclContext* ctx)       override;
    std::any visitUnionDecl(LuxParser::UnionDeclContext* ctx)         override;
    std::any visitEnumDecl(LuxParser::EnumDeclContext* ctx)           override;
    std::any visitExternDecl(LuxParser::ExternDeclContext* ctx)       override;
    std::any visitFunctionDecl(LuxParser::FunctionDeclContext* ctx)   override;
    std::any visitBlock(LuxParser::BlockContext* ctx)                 override;
    std::any visitUseRoot(LuxParser::UseRootContext* ctx)             override;
    std::any visitUseItem(LuxParser::UseItemContext* ctx)             override;
    std::any visitUseGroup(LuxParser::UseGroupContext* ctx)           override;
    std::any visitUseEnumWildcard(LuxParser::UseEnumWildcardContext* ctx) override;
    std::any visitVarDeclStmt(LuxParser::VarDeclStmtContext* ctx)     override;
    std::any visitAssignStmt(LuxParser::AssignStmtContext* ctx)       override;
    std::any visitCompoundAssignStmt(LuxParser::CompoundAssignStmtContext* ctx) override;
    std::any visitFieldAssignStmt(LuxParser::FieldAssignStmtContext* ctx) override;
    std::any visitFieldCompoundAssignStmt(LuxParser::FieldCompoundAssignStmtContext* ctx) override;
    std::any visitIndexFieldAssignStmt(LuxParser::IndexFieldAssignStmtContext* ctx) override;
    std::any visitFieldIndexAssignStmt(LuxParser::FieldIndexAssignStmtContext* ctx) override;
    std::any visitArrowAssignStmt(LuxParser::ArrowAssignStmtContext* ctx) override;
    std::any visitArrowCompoundAssignStmt(LuxParser::ArrowCompoundAssignStmtContext* ctx) override;
    std::any visitCallStmt(LuxParser::CallStmtContext* ctx)           override;
    std::any visitExprStmt(LuxParser::ExprStmtContext* ctx)           override;
    std::any visitReturnStmt(LuxParser::ReturnStmtContext* ctx)       override;
    std::any visitIfStmt(LuxParser::IfStmtContext* ctx)               override;
    std::any visitForInStmt(LuxParser::ForInStmtContext* ctx)         override;
    std::any visitForClassicStmt(LuxParser::ForClassicStmtContext* ctx) override;
    std::any visitBreakStmt(LuxParser::BreakStmtContext* ctx)         override;
    std::any visitContinueStmt(LuxParser::ContinueStmtContext* ctx)   override;
    std::any visitLoopStmt(LuxParser::LoopStmtContext* ctx)           override;
    std::any visitWhileStmt(LuxParser::WhileStmtContext* ctx)         override;
    std::any visitDoWhileStmt(LuxParser::DoWhileStmtContext* ctx)     override;
    std::any visitSwitchStmt(LuxParser::SwitchStmtContext* ctx)       override;
    std::any visitMethodCallExpr(LuxParser::MethodCallExprContext* ctx) override;
    std::any visitArrowMethodCallExpr(LuxParser::ArrowMethodCallExprContext* ctx) override;
    std::any visitTypeSpec(LuxParser::TypeSpecContext* ctx)           override;

    // Expression visitors (labeled alternatives)
    std::any visitIntLitExpr(LuxParser::IntLitExprContext* ctx)       override;
    std::any visitHexLitExpr(LuxParser::HexLitExprContext* ctx)       override;
    std::any visitOctLitExpr(LuxParser::OctLitExprContext* ctx)       override;
    std::any visitBinLitExpr(LuxParser::BinLitExprContext* ctx)       override;
    std::any visitFloatLitExpr(LuxParser::FloatLitExprContext* ctx)   override;
    std::any visitLeadingDotFloatLitExpr(LuxParser::LeadingDotFloatLitExprContext* ctx) override;
    std::any visitBoolLitExpr(LuxParser::BoolLitExprContext* ctx)     override;
    std::any visitCharLitExpr(LuxParser::CharLitExprContext* ctx)     override;
    std::any visitStrLitExpr(LuxParser::StrLitExprContext* ctx)       override;
    std::any visitCStrLitExpr(LuxParser::CStrLitExprContext* ctx)   override;
    std::any visitIdentExpr(LuxParser::IdentExprContext* ctx)         override;
    std::any visitArrayLitExpr(LuxParser::ArrayLitExprContext* ctx)   override;
    std::any visitListCompExpr(LuxParser::ListCompExprContext* ctx)   override;
    std::any visitIndexExpr(LuxParser::IndexExprContext* ctx)         override;
    std::any visitStructLitExpr(LuxParser::StructLitExprContext* ctx) override;
    std::any visitStructPosInitExpr(LuxParser::StructPosInitExprContext* ctx) override;
    std::any visitFieldAccessExpr(LuxParser::FieldAccessExprContext* ctx) override;
    std::any visitArrowAccessExpr(LuxParser::ArrowAccessExprContext* ctx) override;
    std::any visitEnumAccessExpr(LuxParser::EnumAccessExprContext* ctx) override;
    std::any visitGenericEnumAccessExpr(LuxParser::GenericEnumAccessExprContext* ctx) override;
    std::any visitQualifiedStructPosInitExpr(LuxParser::QualifiedStructPosInitExprContext* ctx) override;
    std::any visitQualifiedStructNamedInitExpr(LuxParser::QualifiedStructNamedInitExprContext* ctx) override;
    std::any visitGenericEnumNamedVariantExpr(LuxParser::GenericEnumNamedVariantExprContext* ctx) override;
    std::any visitGenericEnumPosVariantExpr(LuxParser::GenericEnumPosVariantExprContext* ctx) override;
    std::any visitStaticMethodCallExpr(LuxParser::StaticMethodCallExprContext* ctx) override;
    std::any visitNullLitExpr(LuxParser::NullLitExprContext* ctx)     override;
    std::any visitAddrOfExpr(LuxParser::AddrOfExprContext* ctx)       override;
    std::any visitDerefExpr(LuxParser::DerefExprContext* ctx)         override;
    std::any visitDerefAssignStmt(LuxParser::DerefAssignStmtContext* ctx) override;
    std::any visitDerefCompoundAssignStmt(LuxParser::DerefCompoundAssignStmtContext* ctx) override;
    std::any visitTypeAliasDecl(LuxParser::TypeAliasDeclContext* ctx) override;
    std::any visitFnCallExpr(LuxParser::FnCallExprContext* ctx)       override;
    std::any visitNegExpr(LuxParser::NegExprContext* ctx)             override;
    std::any visitMulExpr(LuxParser::MulExprContext* ctx)             override;
    std::any visitAddSubExpr(LuxParser::AddSubExprContext* ctx)       override;
    std::any visitRelExpr(LuxParser::RelExprContext* ctx)             override;
    std::any visitEqExpr(LuxParser::EqExprContext* ctx)               override;
    std::any visitLogicalNotExpr(LuxParser::LogicalNotExprContext* ctx) override;
    std::any visitLogicalAndExpr(LuxParser::LogicalAndExprContext* ctx) override;
    std::any visitLogicalOrExpr(LuxParser::LogicalOrExprContext* ctx) override;
    std::any visitBitNotExpr(LuxParser::BitNotExprContext* ctx)       override;
    std::any visitLshiftExpr(LuxParser::LshiftExprContext* ctx)       override;
    std::any visitRshiftExpr(LuxParser::RshiftExprContext* ctx)       override;
    std::any visitBitAndExpr(LuxParser::BitAndExprContext* ctx)       override;
    std::any visitBitXorExpr(LuxParser::BitXorExprContext* ctx)       override;
    std::any visitBitOrExpr(LuxParser::BitOrExprContext* ctx)         override;
    std::pair<llvm::Value*, llvm::Type*>
        resolveIncrDecrTarget(LuxParser::ExpressionContext* expr);
    llvm::Value* resolveMethodReceiverAddress(LuxParser::ExpressionContext* expr);
    std::any visitPreIncrExpr(LuxParser::PreIncrExprContext* ctx)     override;
    std::any visitPreDecrExpr(LuxParser::PreDecrExprContext* ctx)     override;
    std::any visitPostIncrExpr(LuxParser::PostIncrExprContext* ctx)   override;
    std::any visitPostDecrExpr(LuxParser::PostDecrExprContext* ctx)   override;
    std::any visitCastExpr(LuxParser::CastExprContext* ctx)           override;
    std::any visitSizeofExpr(LuxParser::SizeofExprContext* ctx)       override;
    std::any visitTypeofExpr(LuxParser::TypeofExprContext* ctx)      override;
    std::any visitTernaryExpr(LuxParser::TernaryExprContext* ctx)     override;
    std::any visitIsExpr(LuxParser::IsExprContext* ctx)               override;
    std::any visitNullCoalExpr(LuxParser::NullCoalExprContext* ctx)   override;
    std::any visitRangeExpr(LuxParser::RangeExprContext* ctx)         override;
    std::any visitRangeInclExpr(LuxParser::RangeInclExprContext* ctx) override;
    std::any visitSpreadExpr(LuxParser::SpreadExprContext* ctx)       override;
    std::any visitParenExpr(LuxParser::ParenExprContext* ctx)         override;
    std::any visitTupleLitExpr(LuxParser::TupleLitExprContext* ctx)   override;
    std::any visitTupleIndexExpr(LuxParser::TupleIndexExprContext* ctx) override;
    std::any visitChainedTupleIndexExpr(LuxParser::ChainedTupleIndexExprContext* ctx) override;
    std::any visitTupleArrowIndexExpr(LuxParser::TupleArrowIndexExprContext* ctx) override;
    std::any visitChainedTupleArrowIndexExpr(LuxParser::ChainedTupleArrowIndexExprContext* ctx) override;
    std::any visitSpawnExpr(LuxParser::SpawnExprContext* ctx)         override;
    std::any visitAwaitExpr(LuxParser::AwaitExprContext* ctx)         override;
    std::any visitLockStmt(LuxParser::LockStmtContext* ctx)           override;
    std::any visitTryCatchStmt(LuxParser::TryCatchStmtContext* ctx)   override;
    std::any visitThrowStmt(LuxParser::ThrowStmtContext* ctx)         override;
    std::any visitTryExpr(LuxParser::TryExprContext* ctx)             override;
    std::any visitCatchUnwrapExpr(LuxParser::CatchUnwrapExprContext* ctx) override;
    std::any visitPropagateExpr(LuxParser::PropagateExprContext* ctx) override;
    std::any visitExtendDecl(LuxParser::ExtendDeclContext* ctx)        override;
    std::any visitDeferStmt(LuxParser::DeferStmtContext* ctx)           override;
    std::any visitNakedBlockStmt(LuxParser::NakedBlockStmtContext* ctx) override;
    std::any visitInlineBlockStmt(LuxParser::InlineBlockStmtContext* ctx) override;
    std::any visitScopeBlockStmt(LuxParser::ScopeBlockStmtContext* ctx) override;
    // Generic expression visitors
    std::any visitGenericFnCallExpr(LuxParser::GenericFnCallExprContext* ctx) override;
    std::any visitGenericStaticMethodCallExpr(LuxParser::GenericStaticMethodCallExprContext* ctx) override;
    std::any visitGenericQualifiedFnCallExpr(LuxParser::GenericQualifiedFnCallExprContext* ctx) override;
    std::any visitGenericStructLitExpr(LuxParser::GenericStructLitExprContext* ctx) override;
    std::any visitGenericStructPosInitExpr(LuxParser::GenericStructPosInitExprContext* ctx) override;

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
        std::vector<unsigned> fixedArraySizes; // declared [N] sizes (incl. pointer-to-array)
    };
    std::unordered_map<std::string, VarInfo> locals_;

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

    // Intrinsic functions (lux::core::trap, etc.)
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
        LuxParser::CallStmtContext* callCtx = nullptr;
        LuxParser::ExprStmtContext* exprCtx = nullptr;
        LuxParser::ScopeCallbackContext* scopeCbCtx = nullptr;
    };
    std::vector<DeferredStmt> deferStack_;

    // Spawn counter for unique wrapper function names
    unsigned spawnCounter_ = 0;

    // Struct methods registered via `extend` blocks
    // structName → { methodName → Function* }
    std::unordered_map<std::string,
        std::unordered_map<std::string, llvm::Function*>> structMethods_;

    // Static struct methods (no &self)
    std::unordered_map<std::string,
        std::unordered_map<std::string, llvm::Function*>> staticStructMethods_;

    // Namespace context for cross-file resolution
    const NamespaceRegistry* nsRegistry_ = nullptr;
    std::string currentNamespace_;
    std::string currentFile_;

    // Maps unmangled symbol name → mangled LLVM function name
    // Built during visitProgram to resolve calls efficiently.
    std::unordered_map<std::string, std::string> callTargetMap_;

    // User imports: symbol name → source namespace
    std::unordered_map<std::string, std::string> userImports_;

    // Injected enum variant names from `use EnumType::*;`
    struct InjectedEnumVariant {
        const TypeInfo* enumType;
        const EnumVariantInfo* variantInfo;
    };
    std::unordered_map<std::string, InjectedEnumVariant> enumVariantImports_;

    // C bindings from parsed #include headers
    const CBindings* cBindings_ = nullptr;

    // C enum constants: qualified name → integer value
    std::unordered_map<std::string, int64_t> cEnumConstants_;

    // C float constants: name → double value
    std::unordered_map<std::string, double> cFloatConstants_;

    // C string constants: name → string value
    std::unordered_map<std::string, std::string> cStringConstants_;

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
    void registerCrossFileSymbols(LuxParser::ProgramContext* ctx);
    void declareExternFunction(const std::string& mangledName,
                               LuxParser::FunctionDeclContext* decl);

    // Forward-declare a user function (signature only, no body)
    void forwardDeclareFunction(LuxParser::FunctionDeclContext* decl);
    std::string resolveCallTarget(const std::string& name) const;

    // Recursively coerce scalars/aggregates to a target LLVM type.
    llvm::Value* coerceValueToType(llvm::Value* value,
                                   llvm::Type* targetType,
                                   bool assumeSignedInt = true);

    // ── User-defined generics ──────────────────────────────────────────────
    // Generic struct and function template registries
    struct GenericStructTemplate {
        std::vector<std::string>      typeParams;
        LuxParser::StructDeclContext* decl;
    };
    struct GenericUnionTemplate {
        std::vector<std::string>     typeParams;
        LuxParser::UnionDeclContext* decl;
    };
    struct GenericEnumTemplate {
        std::vector<std::string>    typeParams;
        LuxParser::EnumDeclContext* decl;
    };
    struct GenericFuncTemplate {
        std::vector<std::string>        typeParams;
        LuxParser::FunctionDeclContext* decl;
    };
    struct GenericExtendTemplate {
        std::vector<std::string>      typeParams;
        LuxParser::ExtendDeclContext*  decl;
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
        LuxParser::TypeSpecContext* ctx,
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
        const std::vector<LuxParser::ParamContext*>& formalParams,
        const std::vector<const TypeInfo*>& argTypes);

    bool unifyGenericTypeArg(
        LuxParser::TypeSpecContext* formalType,
        const TypeInfo* actualType,
        const std::unordered_set<std::string>& genericParams,
        std::unordered_map<std::string, const TypeInfo*>& inferred);

    // Helpers
    const TypeInfo*    resolveTypeInfo(LuxParser::TypeSpecContext* ctx);
    const TypeInfo*    resolveExprTypeInfo(LuxParser::ExpressionContext* ctx);
    unsigned           resolveExprArrayDims(LuxParser::ExpressionContext* ctx);
    bool               isPointerType(LuxParser::TypeSpecContext* ctx);
    unsigned           countArrayDims(LuxParser::TypeSpecContext* ctx);
    llvm::ArrayType*   buildTargetArrayType(llvm::Type* srcTy, llvm::Type* elemTy);
    void               storeArrayElements(llvm::Value* src, llvm::Value* destPtr,
                                          llvm::Type* destArrTy, const TypeInfo* elemTI,
                                          unsigned dims);
    llvm::FunctionCallee declareBuiltin(const std::string& name,
                                        llvm::Type* retType,
                                        llvm::ArrayRef<llvm::Type*> argTypes);
    llvm::Type*         getEnumVariantPayloadType(const EnumVariantInfo& variantInfo);
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
                            LuxParser::ArrayLitExprContext* arrLit,
                            const TypeInfo* vecType,
                            const std::string& tempNameHint = "vec_lit");
    void                emitBlockExitCleanups(const std::unordered_map<std::string, VarInfo>& savedLocals);
    bool                isDropTrackedLocal(const VarInfo& info) const;
    bool                isBorrowedStringValueExpr(LuxParser::ExpressionContext* expr) const;
    void                consumeLocalByName(const std::string& name);
    void                consumeExprIfOwnedLocal(LuxParser::ExpressionContext* expr);
    void                emitScopeCallback(LuxParser::ScopeCallbackContext* ctx);
    void                emitDivByZeroGuard(llvm::Value* divisor,
                                           antlr4::Token* opToken);
    bool                requireArgs(const std::string& funcName,
                                    const std::vector<llvm::Value*>& args,
                                    size_t expected);
    llvm::Value*        castValue(std::any result);
    llvm::Value*        toBool(llvm::Value* value,
                                const TypeInfo* valueTI = nullptr,
                                const std::string& name = "tobool");
    std::pair<llvm::Value*, llvm::Value*>
                       promoteArithmetic(llvm::Value* lhs, llvm::Value* rhs);
};
