#include "IRBuilder/IRGen.h"
#include "IRBuilder/IRGen_helpers.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <vector>
#include <unordered_map>
#include <optional>
#include <filesystem>
#include <fstream>

#ifdef LLVM_VERSION_15_OR_NEWER
#  include <llvm/TargetParser/Host.h>
#else
#  include <llvm/Support/Host.h>
#endif

#include "generated/LucisLexer.h"
#include "namespace/ModuleRegistry.h"
#include "ffi/CBindings.h"
#include "ffi/CMacroEval.h"

#include <cctype>
#include <iostream>



namespace {


static bool evalMacroAdd(const std::vector<std::string>& toks, size_t& pos, int64_t& val);



static bool evalMacroPrimary(const std::vector<std::string>& toks, size_t& pos, int64_t& val) {
    if (pos >= toks.size()) return false;
    if (toks[pos] == "-") { pos++; if (!evalMacroPrimary(toks, pos, val)) return false; val = -val; return true; }
    if (toks[pos] == "~") { pos++; if (!evalMacroPrimary(toks, pos, val)) return false; val = ~val; return true; }
    if (toks[pos] == "(") {
        pos++;
        if (!evalMacroAdd(toks, pos, val)) return false;
        if (pos >= toks.size() || toks[pos] != ")") return false;
        pos++;
        return true;
    }
    try {
        size_t n = 0;
        if (toks[pos].size() > 2 && toks[pos][0] == '0' && (toks[pos][1] == 'x' || toks[pos][1] == 'X'))
            val = std::stoll(toks[pos], &n, 16);
        else if (toks[pos].size() > 1 && toks[pos][0] == '0')
            val = std::stoll(toks[pos], &n, 8);
        else
            val = std::stoll(toks[pos], &n, 10);
        if (n == toks[pos].size()) { pos++; return true; }
    } catch (...) {}
    return false;
}
static bool evalMacroMul(const std::vector<std::string>& toks, size_t& pos, int64_t& val) {
    if (!evalMacroPrimary(toks, pos, val)) return false;
    while (pos < toks.size()) {
        if (toks[pos] == "*") { pos++; int64_t rhs; if (!evalMacroPrimary(toks, pos, rhs)) return false; val *= rhs; }
        else if (toks[pos] == "/") { pos++; int64_t rhs; if (!evalMacroPrimary(toks, pos, rhs)) return false; val /= rhs; }
        else if (toks[pos] == "%") { pos++; int64_t rhs; if (!evalMacroPrimary(toks, pos, rhs)) return false; val %= rhs; }
        else break;
    }
    return true;
}
static bool evalMacroAdd(const std::vector<std::string>& toks, size_t& pos, int64_t& val) {
    if (!evalMacroMul(toks, pos, val)) return false;
    while (pos < toks.size()) {
        if (toks[pos] == "+") { pos++; int64_t rhs; if (!evalMacroMul(toks, pos, rhs)) return false; val += rhs; }
        else if (toks[pos] == "-") { pos++; int64_t rhs; if (!evalMacroMul(toks, pos, rhs)) return false; val -= rhs; }
        else break;
    }
    return true;
}
static bool evalMacroShift(const std::vector<std::string>& toks, size_t& pos, int64_t& val) {
    if (!evalMacroAdd(toks, pos, val)) return false;
    while (pos < toks.size()) {
        if (toks[pos] == "<<") { pos++; int64_t rhs; if (!evalMacroAdd(toks, pos, rhs)) return false; val <<= rhs; }
        else if (toks[pos] == ">>") { pos++; int64_t rhs; if (!evalMacroAdd(toks, pos, rhs)) return false; val >>= rhs; }
        else break;
    }
    return true;
}
}



std::any IRGen::visitFnCallExpr(LucisParser::FnCallExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* baseExpr = ctx->expression();

    auto cleanupTempArg = [&](LucisParser::ExpressionContext* argExpr,
                              llvm::Value* argVal,
                              llvm::Type* expectedParamTy) {
        if (!argExpr || !argVal) return;
        // Ident: the caller's local owns the string — never free here.
        if (dynamic_cast<LucisParser::IdentExprContext*>(argExpr)) return;
        // Field access (a.b) and index (v[i]) produce views owned by the
        // containing struct/vec — freeing here dangles the field/element.
        if (dynamic_cast<LucisParser::FieldAccessExprContext*>(argExpr)) return;
        if (dynamic_cast<LucisParser::IndexExprContext*>(argExpr)) return;

        auto* argTI = resolveExprTypeInfo(argExpr);
        if (resolveExprArrayDims(argExpr) > 0) return;

        auto* voidTy = llvm::Type::getVoidTy(*context_);
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);

        bool isStringArg = false;
        if (argTI && argTI->kind == TypeKind::String) {
            isStringArg = true;
        } else if (!argTI && expectedParamTy && expectedParamTy->isStructTy()) {
            auto* st = llvm::cast<llvm::StructType>(expectedParamTy);
            if (st->getNumElements() == 2 &&
                st->getElementType(0)->isPointerTy() &&
                st->getElementType(1)->isIntegerTy()) {
                isStringArg = true;
            }
        }

        if (isStringArg) {
            if (isBorrowedStringExpr(argExpr)) return;
            auto* strPtr = builder_->CreateExtractValue(argVal, 0, "tmp_arg_ptr");
            auto* strLen = builder_->CreateExtractValue(argVal, 1, "tmp_arg_len");
            auto callee = declareBuiltin("lucis_freeStr", voidTy, {ptrTy, usizeTy});
            builder_->CreateCall(callee, {strPtr, strLen});
            return;
        }

        if (!argTI) return;

        if (argTI->kind != TypeKind::Extended) return;

        std::string freeFuncName;
        if (argTI->extendedKind == "Vec") {
            auto suffix = getVecSuffix(argTI->elementType ? argTI->elementType : argTI);
            freeFuncName = "lucis_vec_free_" + suffix;
        } else if (argTI->extendedKind == "Map") {
            freeFuncName = "lucis_map_free_" + argTI->builtinSuffix;
        } else if (argTI->extendedKind == "Set") {
            auto suffix = argTI->elementType
                              ? (argTI->elementType->builtinSuffix.empty()
                                     ? "raw" : argTI->elementType->builtinSuffix)
                              : argTI->builtinSuffix;
            freeFuncName = "lucis_set_free_" + suffix;
        } else {
            return;
        }

        auto* extTy = argTI->toLLVMType(*context_, module_->getDataLayout());
        auto* tmpAlloca = builder_->CreateAlloca(extTy, nullptr, "tmp_arg_ext");
        builder_->CreateStore(argVal, tmpAlloca);
        auto callee = declareBuiltin(freeFuncName, voidTy, {ptrTy});
        builder_->CreateCall(callee, {tmpAlloca});
    };

    // Resolve the function TypeInfo from the base expression
    const TypeInfo* fnTI = nullptr;
    llvm::Function* directFn = nullptr;
    std::string calleeName;

    // Check if base is a simple identifier
    if (auto* identBase = dynamic_cast<LucisParser::IdentExprContext*>(baseExpr)) {
        calleeName = identBase->IDENTIFIER()->getText();

        // ── Comptime function call ────────────────────────────────────
        if (comptimeEngine_ && comptimeEngine_->isReady() &&
            comptimeEngine_->registry().isComptime(calleeName)) {
            // Get argument values as LLVM constants
            std::vector<llvm::Value*> argVals;
            if (auto* argList = ctx->argList()) {
                for (auto* exprCtx : argList->expression())
                    argVals.push_back(castValue(visit(exprCtx)));
            }
            // Convert to ComptimeValue and evaluate
            std::vector<ComptimeValue> cvArgs;
            for (auto* av : argVals) {
                if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(av)) {
                    cvArgs.push_back(
                        ci->getType()->isIntegerTy(1)
                            ? ComptimeValue::boolVal(ci->isOne())
                            : ComptimeValue::intVal(ci->getSExtValue()));
                } else if (auto* cf = llvm::dyn_cast<llvm::ConstantFP>(av)) {
                    cvArgs.push_back(ComptimeValue::floatVal(
                        cf->getValueAPF().convertToDouble()));
                } else {
                    return static_cast<llvm::Value*>(av); // non-constant, fallback
                }
            }
            // Resolve return type from the comptime function declaration
            auto* decl = comptimeEngine_->registry().lookup(calleeName);
            auto* retTypeSpec = static_cast<LucisParser::FunctionDeclContext*>(decl)->typeSpec();
            auto getLLVMType = [&](const std::string& name) -> llvm::Type* {
                if (name == "void")   return llvm::Type::getVoidTy(*context_);
                if (name == "bool")   return llvm::Type::getInt1Ty(*context_);
                if (name == "int8" || name == "char" || name == "byte")  return llvm::Type::getInt8Ty(*context_);
                if (name == "int16" || name == "uint16") return llvm::Type::getInt16Ty(*context_);
                if (name == "int32" || name == "uint32" || name == "int") return llvm::Type::getInt32Ty(*context_);
                if (name == "int64" || name == "uint64" || name == "isize" || name == "usize")
                    return llvm::Type::getInt64Ty(*context_);
                if (name == "float32" || name == "float") return llvm::Type::getFloatTy(*context_);
                if (name == "float64" || name == "double") return llvm::Type::getDoubleTy(*context_);
                return llvm::Type::getInt32Ty(*context_);
            };
            std::string retTypeName;
            if (retTypeSpec->IDENTIFIER())
                retTypeName = retTypeSpec->IDENTIFIER()->getText();
            else if (retTypeSpec->primitiveType()) {
                if (retTypeSpec->primitiveType()->BOOL()) retTypeName = "bool";
                else if (retTypeSpec->primitiveType()->INT32()) retTypeName = "int32";
                else if (retTypeSpec->primitiveType()->INT64()) retTypeName = "int64";
                else if (retTypeSpec->primitiveType()->FLOAT32()) retTypeName = "float32";
                else if (retTypeSpec->primitiveType()->FLOAT64()) retTypeName = "float64";
                else if (retTypeSpec->primitiveType()->VOID()) retTypeName = "void";
                else if (retTypeSpec->primitiveType()->CHAR()) retTypeName = "char";
                else if (retTypeSpec->primitiveType()->UINT32()) retTypeName = "uint32";
                else if (retTypeSpec->primitiveType()->UINT64()) retTypeName = "uint64";
                else if (retTypeSpec->primitiveType()->INT8()) retTypeName = "int8";
                else if (retTypeSpec->primitiveType()->UINT8()) retTypeName = "uint8";
                else if (retTypeSpec->primitiveType()->INT16()) retTypeName = "int16";
                else if (retTypeSpec->primitiveType()->UINT16()) retTypeName = "uint16";
                else if (retTypeSpec->primitiveType()->ISIZE()) retTypeName = "isize";
                else if (retTypeSpec->primitiveType()->USIZE()) retTypeName = "usize";
                else retTypeName = "int32";
            } else {
                retTypeName = "int32";
            }
            auto result = comptimeEngine_->evaluate(decl, cvArgs);
            if (result.kind() == ComptimeValue::Kind::Int) {
                if (retTypeName == "bool") {
                    return static_cast<llvm::Value*>(
                        llvm::ConstantInt::get(
                            llvm::Type::getInt1Ty(*context_),
                            result.asInt() != 0));
                }
                return static_cast<llvm::Value*>(
                    llvm::ConstantInt::get(
                        getLLVMType(retTypeName),
                        result.asInt(), retTypeName.find('u') == std::string::npos));
            }
            if (result.kind() == ComptimeValue::Kind::Bool) {
                return static_cast<llvm::Value*>(
                    llvm::ConstantInt::get(
                        llvm::Type::getInt1Ty(*context_),
                        result.asBool() ? 1 : 0));
            }
            if (result.kind() == ComptimeValue::Kind::Float) {
                return static_cast<llvm::Value*>(
                    llvm::ConstantFP::get(
                        getLLVMType(retTypeName), result.asFloat()));
            }
            if (result.kind() == ComptimeValue::Kind::Void) {
                return static_cast<llvm::Value*>(
                    llvm::Constant::getNullValue(
                        llvm::Type::getInt32Ty(*context_)));
            }
            if (result.kind() == ComptimeValue::Kind::String) {
                // Build a proper {i8*, i64} string struct (like visitStrLitExpr does)
                std::string s = result.asString();
                auto* strGlobal = builder_->CreateGlobalString(s,
                    ".comptime_str", 0, module_);
                auto* strTy = typeRegistry_.lookup("string")
                    ->toLLVMType(*context_, module_->getDataLayout());
                auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                auto* len = llvm::ConstantInt::get(usizeTy, s.size());
                llvm::Value* strStruct = llvm::UndefValue::get(strTy);
                strStruct = builder_->CreateInsertValue(strStruct, strGlobal, 0, "cmpt_str_ptr");
                strStruct = builder_->CreateInsertValue(strStruct, len,       1, "cmpt_str_len");
                return static_cast<llvm::Value*>(strStruct);
            }
        }

        // ── Function-like macro: KEY_F(1), NCURSES_BITS(1,13) etc. ──
        const CFunctionLikeMacro* flm = nullptr;
        if (cBindings_) flm = cBindings_->findFunctionLikeMacro(calleeName);
        if (!flm) flm = cMacroBindings_.findFunctionLikeMacro(calleeName);
        if (flm) {
                // Collect argument values as constants
                std::vector<int64_t> argVals;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression()) {
                        auto argVal = visit(exprCtx);
                        if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(castValue(argVal))) {
                            argVals.push_back(ci->getSExtValue());
                        } else {
                            break; // non-constant argument
                        }
                    }
                }
                if (argVals.size() != flm->paramNames.size())
                    return static_cast<llvm::Value*>(
                        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));

                // Substitute parameter names with argument values in body tokens
                std::vector<std::string> evalTokens;
                for (auto& tok : flm->bodyTokens) {
                    bool isParam = false;
                    for (size_t i = 0; i < flm->paramNames.size(); i++) {
                        if (tok == flm->paramNames[i]) {
                            evalTokens.push_back(std::to_string(argVals[i]));
                            isParam = true;
                            break;
                        }
                    }
                    if (!isParam)
                        evalTokens.push_back(tok);
                }

                // Resolve identifiers from cEnumConstants_ and similar
                for (auto& tok : evalTokens) {
                    auto eit = cEnumConstants_.find(tok);
                    if (eit != cEnumConstants_.end()) {
                        tok = std::to_string(eit->second);
                    }
                }

                // Evaluate constant expression
                int64_t result = 0;
                size_t pos = 0;
                if (evalMacroAdd(evalTokens, pos, result) && pos == evalTokens.size()) {
                    return static_cast<llvm::Value*>(
                        llvm::ConstantInt::get(
                            llvm::Type::getInt32Ty(*context_), result, true));
                }
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }

        // ── Global builtins (value-returning: toInt, toFloat, toBool, toString) ──
        // Check generic function templates first to allow user-defined overrides
        if (globalBuiltins_.count(calleeName) && genericFuncTemplates_.count(calleeName)) {
            std::vector<const TypeInfo*> argTypes;
            if (auto* argList = ctx->argList()) {
                for (auto* exprCtx : argList->expression())
                    argTypes.push_back(resolveExprTypeInfo(exprCtx));
            }
            auto fit = genericFuncTemplates_.find(calleeName);
            if (fit != genericFuncTemplates_.end()) {
                std::vector<LucisParser::ParamContext*> formalParams;
                if (auto* paramList = fit->second.decl->paramList())
                    formalParams = paramList->param();
                // Check for typed variadic last param
                bool hasVariadic = false;
                size_t fixedCount = formalParams.size();
                if (!formalParams.empty()) {
                    auto* last = formalParams.back();
                    hasVariadic = last && last->SPREAD() && last->typeSpec();
                    if (hasVariadic)
                        fixedCount = formalParams.size() - 1;
                }
                bool argCountOk = hasVariadic
                    ? argTypes.size() >= fixedCount
                    : argTypes.size() == formalParams.size();
                if (argCountOk &&
                    inferGenericTypeArgs(fit->second.typeParams, formalParams, argTypes))
                    goto generic_template_handled;
            }
        }
        if (globalBuiltins_.count(calleeName)) {
            auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto* strTy   = typeRegistry_.lookup("string")
                                ->toLLVMType(*context_, module_->getDataLayout());

            // Collect arguments
            std::vector<llvm::Value*> args;
            if (auto* argList = ctx->argList()) {
                for (auto* exprCtx : argList->expression())
                    args.push_back(castValue(visit(exprCtx)));
            }

            if (calleeName == "toInt") {
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* strPtr = builder_->CreateExtractValue(args[0], 0, "toInt_ptr");
                auto* strLen = builder_->CreateExtractValue(args[0], 1, "toInt_len");
                auto callee = declareBuiltin("lucis_toInt",
                    llvm::Type::getInt64Ty(*context_), {ptrTy, usizeTy});
                auto* result = builder_->CreateCall(callee, {strPtr, strLen}, "toInt");
                return static_cast<llvm::Value*>(result);
            }
            if (calleeName == "toFloat") {
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* strPtr = builder_->CreateExtractValue(args[0], 0, "toFloat_ptr");
                auto* strLen = builder_->CreateExtractValue(args[0], 1, "toFloat_len");
                auto callee = declareBuiltin("lucis_toFloat",
                    llvm::Type::getDoubleTy(*context_), {ptrTy, usizeTy});
                auto* result = builder_->CreateCall(callee, {strPtr, strLen}, "toFloat");
                return static_cast<llvm::Value*>(result);
            }
            if (calleeName == "toBool") {
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* strPtr = builder_->CreateExtractValue(args[0], 0, "toBool_ptr");
                auto* strLen = builder_->CreateExtractValue(args[0], 1, "toBool_len");
                auto callee = declareBuiltin("lucis_toBool",
                    llvm::Type::getInt1Ty(*context_), {ptrTy, usizeTy});
                auto* result = builder_->CreateCall(callee, {strPtr, strLen}, "toBool");
                return static_cast<llvm::Value*>(result);
            }
            if (calleeName == "toString") {
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* arg     = args[0];
                auto* argType = arg->getType();

                // Detect type info via variable lookup
                const TypeInfo* argTI = nullptr;
                auto argExprs = ctx->argList()->expression();
                if (auto* argIdent = dynamic_cast<LucisParser::IdentExprContext*>(argExprs[0])) {
                    auto varIt = locals_.find(argIdent->IDENTIFIER()->getText());
                    if (varIt != locals_.end())
                        argTI = varIt->second.typeInfo;
                }

                // Detect char literal directly from the AST
                bool isCharLit = dynamic_cast<LucisParser::CharLitExprContext*>(argExprs[0]) != nullptr;

                std::string suffix;
                if (argTI) {
                    suffix = argTI->builtinSuffix;
                } else if (isCharLit)                 { suffix = "char"; }
                else if (argType->isIntegerTy(1))     { suffix = "bool"; }
                else if (argType->isIntegerTy(8))     { suffix = "i8"; }
                else if (argType->isIntegerTy(16))    { suffix = "i16"; }
                else if (argType->isIntegerTy(32))    { suffix = "i32"; }
                else if (argType->isIntegerTy(64))    { suffix = "i64"; }
                else if (argType->isFloatTy())        { suffix = "f32"; }
                else if (argType->isDoubleTy())       { suffix = "f64"; }
                else                                  { suffix = "i32"; }

                auto cFuncName = "lucis_toString_" + suffix;

                // toString returns a struct {ptr, i64} matching the string ABI
                auto* retStructTy = llvm::StructType::get(*context_,
                    {ptrTy, usizeTy});
                auto callee = declareBuiltin(cFuncName, retStructTy, {argType});
                auto* retVal = builder_->CreateCall(callee, {arg}, "toStr");

                // Extract ptr and len, build lucis string fat pointer
                auto* retPtr = builder_->CreateExtractValue(retVal, 0, "toStr_ptr");
                auto* retLen = builder_->CreateExtractValue(retVal, 1, "toStr_len");
                llvm::Value* strStruct = llvm::UndefValue::get(strTy);
                strStruct = builder_->CreateInsertValue(strStruct, retPtr, 0, "str_ptr");
                strStruct = builder_->CreateInsertValue(strStruct, retLen, 1, "str_len");
                return static_cast<llvm::Value*>(strStruct);
            }

            // ── cstr(string) -> *char ──
            if (calleeName == "cstr") {
                auto* strPtr = builder_->CreateExtractValue(args[0], 0, "cstr_ptr");
                auto* strLen = builder_->CreateExtractValue(args[0], 1, "cstr_len");
                auto callee = declareBuiltin("lucis_cstr", ptrTy, {ptrTy, usizeTy});
                auto* result = builder_->CreateCall(callee, {strPtr, strLen}, "cstr");
                return static_cast<llvm::Value*>(result);
            }

            // ── fromCStr(*char) -> string ──
            if (calleeName == "fromCStr") {
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* retStructTy = llvm::StructType::get(*context_, {ptrTy, usizeTy});
                auto callee = declareBuiltin("lucis_fromCStr", retStructTy, {ptrTy});
                auto* retVal = builder_->CreateCall(callee, {args[0]}, "fromCStr");
                auto* retPtr = builder_->CreateExtractValue(retVal, 0, "fromCStr_ptr");
                auto* retLen = builder_->CreateExtractValue(retVal, 1, "fromCStr_len");
                llvm::Value* strStruct = llvm::UndefValue::get(strTy);
                strStruct = builder_->CreateInsertValue(strStruct, retPtr, 0, "str_ptr");
                strStruct = builder_->CreateInsertValue(strStruct, retLen, 1, "str_len");
                return static_cast<llvm::Value*>(strStruct);
            }

            // ── fromCStrCopy(*char) -> string ──
            if (calleeName == "fromCStrCopy") {
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* retStructTy = llvm::StructType::get(*context_, {ptrTy, usizeTy});
                auto callee = declareBuiltin("lucis_fromCStrCopy", retStructTy, {ptrTy});
                auto* retVal = builder_->CreateCall(callee, {args[0]}, "fromCStrCopy");
                auto* retPtr = builder_->CreateExtractValue(retVal, 0, "fromCStrCopy_ptr");
                auto* retLen = builder_->CreateExtractValue(retVal, 1, "fromCStrCopy_len");
                llvm::Value* strStruct = llvm::UndefValue::get(strTy);
                strStruct = builder_->CreateInsertValue(strStruct, retPtr, 0, "str_ptr");
                strStruct = builder_->CreateInsertValue(strStruct, retLen, 1, "str_len");
                return static_cast<llvm::Value*>(strStruct);
            }

            // ── freeStr(string) -> void ──
            if (calleeName == "freeStr") {
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* strPtr = builder_->CreateExtractValue(args[0], 0, "freeStr_ptr");
                auto* strLen = builder_->CreateExtractValue(args[0], 1, "freeStr_len");
                auto callee = declareBuiltin("lucis_freeStr", llvm::Type::getVoidTy(*context_), {ptrTy, usizeTy});
                builder_->CreateCall(callee, {strPtr, strLen});
                if (ctx->argList() && !ctx->argList()->expression().empty())
                    consumeExprIfOwnedLocal(ctx->argList()->expression()[0]);
                return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getVoidTy(*context_)));
            }

            // ── fromCStrLen(*char, usize) -> string ──
            if (calleeName == "fromCStrLen") {
                if (!requireArgs(calleeName, args, 2)) return {};
                auto* retStructTy = llvm::StructType::get(*context_, {ptrTy, usizeTy});
                auto* lenArg = args[1];
                if (lenArg->getType() != usizeTy)
                    lenArg = builder_->CreateIntCast(lenArg, usizeTy, false, "fromCStrLen_cast");
                auto callee = declareBuiltin("lucis_fromCStrLen", retStructTy, {ptrTy, usizeTy});
                auto* retVal = builder_->CreateCall(callee, {args[0], lenArg}, "fromCStrLen");
                auto* retPtr = builder_->CreateExtractValue(retVal, 0, "fromCStrLen_ptr");
                auto* retLen = builder_->CreateExtractValue(retVal, 1, "fromCStrLen_len");
                llvm::Value* strStruct = llvm::UndefValue::get(strTy);
                strStruct = builder_->CreateInsertValue(strStruct, retPtr, 0, "str_ptr");
                strStruct = builder_->CreateInsertValue(strStruct, retLen, 1, "str_len");
                return static_cast<llvm::Value*>(strStruct);
            }
        }

        // ── sprintf() as expression: format string with {} placeholders ──
        if (calleeName == "sprintf" && imports_.isImported("sprintf")) {
            auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto* strTy   = typeRegistry_.lookup("string")
                                ->toLLVMType(*context_, module_->getDataLayout());

            std::vector<llvm::Value*> args;
            if (auto* argList = ctx->argList()) {
                for (auto* exprCtx : argList->expression())
                    args.push_back(castValue(visit(exprCtx)));
            }

            // First arg is the format string
            auto* fmtPtr = builder_->CreateExtractValue(args[0], 0, "fmt_ptr");
            auto* fmtLen = builder_->CreateExtractValue(args[0], 1, "fmt_len");

            // Build StringSlice struct type: { ptr, i64 }
            auto* sliceTy = llvm::StructType::get(*context_, {ptrTy, usizeTy});
            auto* i64Ty = llvm::Type::getInt64Ty(*context_);

            llvm::Value* argsPtr;
            llvm::Value* argsCount;

            if (args.size() <= 1) {
                // No format args
                argsPtr   = llvm::ConstantPointerNull::get(ptrTy);
                argsCount = llvm::ConstantInt::get(usizeTy, 0);
            } else {
                // Pack remaining string args into array of {ptr, len} structs
                size_t numArgs = args.size() - 1;
                auto* arrTy = llvm::ArrayType::get(sliceTy, numArgs);
                auto* arrAlloca = builder_->CreateAlloca(arrTy, nullptr, "sprintf.args");
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);

                for (size_t i = 0; i < numArgs; i++) {
                    auto* arg = args[i + 1];
                    auto* argTy = arg->getType();

                    // Convert non-string args to string slices
                    if (argTy != sliceTy) {
                        llvm::Value* converted = nullptr;
                        if (argTy->isIntegerTy(1)) {
                            // bool → "true" / "false"
                            auto* trueStr  = builder_->CreateGlobalString("true", ".str.true", 0, module_);
                            auto* falseStr = builder_->CreateGlobalString("false", ".str.false", 0, module_);
                            auto* trueLen  = llvm::ConstantInt::get(usizeTy, 4);
                            auto* falseLen = llvm::ConstantInt::get(usizeTy, 5);
                            auto* ptr = builder_->CreateSelect(arg, trueStr, falseStr);
                            auto* len = builder_->CreateSelect(arg, trueLen, falseLen);
                            converted = llvm::UndefValue::get(sliceTy);
                            converted = builder_->CreateInsertValue(converted, ptr, 0);
                            converted = builder_->CreateInsertValue(converted, len, 1);
                        } else if (argTy->isIntegerTy()) {
                            // int → lucis_itoa / lucis_utoa
                            auto* ext = builder_->CreateIntCast(arg, i64Ty, true, "icast");
                            auto fn = declareBuiltin("lucis_itoa", sliceTy, {i64Ty});
                            converted = builder_->CreateCall(fn, {ext}, "itoa");
                        } else if (argTy->isFloatingPointTy()) {
                            // float → lucis_ftoa
                            auto* dblTy = llvm::Type::getDoubleTy(*context_);
                            auto* ext = argTy->isFloatTy()
                                ? builder_->CreateFPExt(arg, dblTy, "fext")
                                : arg;
                            auto fn = declareBuiltin("lucis_ftoa", sliceTy, {dblTy});
                            converted = builder_->CreateCall(fn, {ext}, "ftoa");
                        } else if (argTy->isIntegerTy(8)) {
                            // char → 1-byte string
                            auto* buf = builder_->CreateAlloca(
                                llvm::Type::getInt8Ty(*context_), nullptr, "chbuf");
                            builder_->CreateStore(arg, buf);
                            converted = llvm::UndefValue::get(sliceTy);
                            converted = builder_->CreateInsertValue(converted, buf, 0);
                            converted = builder_->CreateInsertValue(converted,
                                llvm::ConstantInt::get(usizeTy, 1), 1);
                        } else {
                            // Fallback: store as-is (will likely be wrong)
                            converted = arg;
                        }
                        arg = converted;
                    }

                    auto* idx = llvm::ConstantInt::get(i64Ty, i);
                    auto* gep = builder_->CreateGEP(arrTy, arrAlloca, {zero, idx});
                    builder_->CreateStore(arg, gep);
                }

                argsPtr   = builder_->CreateGEP(arrTy, arrAlloca,
                                {zero, llvm::ConstantInt::get(i64Ty, 0)}, "args_ptr");
                argsCount = llvm::ConstantInt::get(usizeTy, numArgs);
            }

            auto callee = declareBuiltin("lucis_sprintf", strTy,
                {ptrTy, usizeTy, ptrTy, usizeTy});
            auto* ret = builder_->CreateCall(callee,
                {fmtPtr, fmtLen, argsPtr, argsCount}, "sprintf");

            auto* retPtr = builder_->CreateExtractValue(ret, 0, "sprintf_ptr");
            auto* retLen = builder_->CreateExtractValue(ret, 1, "sprintf_len");
            llvm::Value* strStruct = llvm::UndefValue::get(strTy);
            strStruct = builder_->CreateInsertValue(strStruct, retPtr, 0);
            strStruct = builder_->CreateInsertValue(strStruct, retLen, 1);
            return static_cast<llvm::Value*>(strStruct);
        }

        // ── dbg() as expression: print debug info and return value ──
        if (calleeName == "dbg" && imports_.isImported("dbg")) {
            auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto* i32Ty   = llvm::Type::getInt32Ty(*context_);

            std::vector<llvm::Value*> args;
            if (auto* argList = ctx->argList()) {
                for (auto* exprCtx : argList->expression())
                    args.push_back(castValue(visit(exprCtx)));
            }

            if (args.empty()) {
                std::cerr << "lucis: dbg() requires an argument\n";
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }

            auto* arg     = args[0];
            auto* argType = arg->getType();

            auto fileName = module_->getName().str();
            auto* fileGlobal = builder_->CreateGlobalString(fileName, ".dbg_file", 0, module_);
            auto* fileLen = llvm::ConstantInt::get(usizeTy, fileName.size());
            auto* lineNo  = llvm::ConstantInt::get(i32Ty, ctx->getStart()->getLine());

            const TypeInfo* argTI = nullptr;
            auto argExprs = ctx->argList()->expression();
            if (auto* argIdent = dynamic_cast<LucisParser::IdentExprContext*>(argExprs[0])) {
                auto varIt = locals_.find(argIdent->IDENTIFIER()->getText());
                if (varIt != locals_.end())
                    argTI = varIt->second.typeInfo;
            }

            bool isString = (argTI && argTI->kind == TypeKind::String)
                         || (!argTI && argType->isStructTy());

            if (isString) {
                auto* strPtr = builder_->CreateExtractValue(arg, 0, "dbg_str_ptr");
                auto* strLen = builder_->CreateExtractValue(arg, 1, "dbg_str_len");
                auto callee = declareBuiltin("lucis_dbg_str",
                    llvm::Type::getVoidTy(*context_),
                    {ptrTy, usizeTy, i32Ty, ptrTy, usizeTy});
                builder_->CreateCall(callee, {fileGlobal, fileLen, lineNo, strPtr, strLen});
                return static_cast<llvm::Value*>(arg);
            }

            std::string suffix;
            if (argTI)                            suffix = argTI->builtinSuffix;
            else if (argType->isIntegerTy(1))     suffix = "bool";
            else if (argType->isIntegerTy(8))     suffix = "i8";
            else if (argType->isIntegerTy(16))    suffix = "i16";
            else if (argType->isIntegerTy(32))    suffix = "i32";
            else if (argType->isIntegerTy(64))    suffix = "i64";
            else if (argType->isIntegerTy(128))   suffix = "i128";
            else if (argType->isFloatTy())        suffix = "f32";
            else if (argType->isDoubleTy())       suffix = "f64";
            else                                  suffix = "i32";

            llvm::Value* callArg = arg;
            llvm::Type*  paramType = argType;
            if (argType->isIntegerTy(256)) {
                paramType = llvm::Type::getInt128Ty(*context_);
                callArg   = builder_->CreateTrunc(arg, paramType);
            }

            auto cFuncName = "lucis_dbg_" + suffix;
            auto callee = declareBuiltin(cFuncName,
                llvm::Type::getVoidTy(*context_),
                {ptrTy, usizeTy, i32Ty, paramType});
            builder_->CreateCall(callee, {fileGlobal, fileLen, lineNo, callArg});
            return static_cast<llvm::Value*>(arg);
        }

        // ── std::io functions as expressions ────────────────────────────
        if (imports_.isImported(calleeName)) {
            auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto* voidTy  = llvm::Type::getVoidTy(*context_);
            auto* i1Ty    = llvm::Type::getInt1Ty(*context_);
            auto* i8Ty    = llvm::Type::getInt8Ty(*context_);
            auto* i32Ty   = llvm::Type::getInt32Ty(*context_);
            auto* i64Ty   = llvm::Type::getInt64Ty(*context_);
            auto* f64Ty   = llvm::Type::getDoubleTy(*context_);
            auto* strTy   = llvm::StructType::get(*context_, {ptrTy, usizeTy});

            // Helper lambda: build string fat pointer from C return struct
            auto buildString = [&](llvm::Value* retVal) -> llvm::Value* {
                auto* retPtr = builder_->CreateExtractValue(retVal, 0, "io_str_ptr");
                auto* retLen = builder_->CreateExtractValue(retVal, 1, "io_str_len");
                llvm::Value* str = llvm::UndefValue::get(strTy);
                str = builder_->CreateInsertValue(str, retPtr, 0);
                str = builder_->CreateInsertValue(str, retLen, 1);
                return str;
            };

            // Helper lambda: extract string arg (ptr, len) from fat pointer
            auto extractStringArg = [&](llvm::Value* arg,
                                        std::vector<llvm::Value*>& out) {
                out.push_back(builder_->CreateExtractValue(arg, 0, "io_msg_ptr"));
                out.push_back(builder_->CreateExtractValue(arg, 1, "io_msg_len"));
            };

            // ── No-arg, returns string ──
            if (calleeName == "readLine" || calleeName == "readAll" ||
                calleeName == "readPassword") {
                auto callee = declareBuiltin("lucis_" + calleeName, strTy, {});
                auto* ret = builder_->CreateCall(callee, {}, calleeName);
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── No-arg, returns primitive ──
            if (calleeName == "readChar") {
                auto callee = declareBuiltin("lucis_readChar", i8Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "readChar");
                return static_cast<llvm::Value*>(ret);
            }
            if (calleeName == "readInt") {
                auto callee = declareBuiltin("lucis_readInt", i64Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "readInt");
                return static_cast<llvm::Value*>(ret);
            }
            if (calleeName == "readFloat") {
                auto callee = declareBuiltin("lucis_readFloat", f64Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "readFloat");
                return static_cast<llvm::Value*>(ret);
            }
            if (calleeName == "readBool") {
                auto callee = declareBuiltin("lucis_readBool", i32Ty, {});
                auto* ret32 = builder_->CreateCall(callee, {}, "readBool_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), "readBool");
                return static_cast<llvm::Value*>(ret);
            }
            if (calleeName == "readByte") {
                auto callee = declareBuiltin("lucis_readByte", i8Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "readByte");
                return static_cast<llvm::Value*>(ret);
            }
            if (calleeName == "isEOF") {
                auto callee = declareBuiltin("lucis_isEOF", i32Ty, {});
                auto* ret32 = builder_->CreateCall(callee, {}, "isEOF_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), "isEOF");
                return static_cast<llvm::Value*>(ret);
            }
            if (calleeName == "isTTY") {
                auto callee = declareBuiltin("lucis_isTTY", i32Ty, {});
                auto* ret32 = builder_->CreateCall(callee, {}, "isTTY_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), "isTTY");
                return static_cast<llvm::Value*>(ret);
            }
            if (calleeName == "isStdoutTTY") {
                auto callee = declareBuiltin("lucis_isStdoutTTY", i32Ty, {});
                auto* ret32 = builder_->CreateCall(callee, {}, "isStdoutTTY_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), "isStdoutTTY");
                return static_cast<llvm::Value*>(ret);
            }
            if (calleeName == "isStderrTTY") {
                auto callee = declareBuiltin("lucis_isStderrTTY", i32Ty, {});
                auto* ret32 = builder_->CreateCall(callee, {}, "isStderrTTY_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), "isStderrTTY");
                return static_cast<llvm::Value*>(ret);
            }

            // ── String arg, returns string ──
            if (calleeName == "prompt" || calleeName == "promptPassword") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_" + calleeName, strTy,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, calleeName);
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── String arg, returns int64 ──
            if (calleeName == "promptInt") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_promptInt", i64Ty,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "promptInt");
                return static_cast<llvm::Value*>(ret);
            }

            // ── String arg, returns float64 ──
            if (calleeName == "promptFloat") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_promptFloat", f64Ty,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "promptFloat");
                return static_cast<llvm::Value*>(ret);
            }

            // ── String arg, returns bool ──
            if (calleeName == "promptBool") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_promptBool", i32Ty,
                    {ptrTy, usizeTy});
                auto* ret32 = builder_->CreateCall(callee, callArgs, "promptBool_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), "promptBool");
                return static_cast<llvm::Value*>(ret);
            }

            // ── void functions as expression (flush, flushErr) — return undef ──
            if (calleeName == "flush") {
                auto callee = declareBuiltin("lucis_flush", voidTy, {});
                builder_->CreateCall(callee, {});
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(i32Ty));
            }
            if (calleeName == "flushErr") {
                auto callee = declareBuiltin("lucis_flushErr", voidTy, {});
                builder_->CreateCall(callee, {});
                return static_cast<llvm::Value*>(
                    llvm::UndefValue::get(i32Ty));
            }

            // ── std::math — single-arg float64 → float64 ───────────────────
            static const std::unordered_set<std::string> mathF64Unary = {
                "sqrt", "cbrt", "exp", "exp2", "ln", "log2", "log10",
                "sin", "cos", "tan", "asin", "acos", "atan",
                "sinh", "cosh", "tanh",
                "ceil", "floor", "round", "trunc",
                "toRadians", "toDegrees"
            };
            if (mathF64Unary.count(calleeName)) {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* arg = args[0];
                if (arg->getType()->isIntegerTy())
                    arg = builder_->CreateSIToFP(arg, f64Ty, "math_cast");
                else if (arg->getType()->isFloatTy())
                    arg = builder_->CreateFPExt(arg, f64Ty, "math_ext");
                auto callee = declareBuiltin("lucis_" + calleeName, f64Ty, {f64Ty});
                auto* ret = builder_->CreateCall(callee, {arg}, calleeName);
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::math — two-arg float64 → float64 ──────────────────────
            static const std::unordered_set<std::string> mathF64Binary = {
                "pow", "hypot", "atan2"
            };
            if (mathF64Binary.count(calleeName)) {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                for (auto*& a : args) {
                    if (a->getType()->isIntegerTy())
                        a = builder_->CreateSIToFP(a, f64Ty, "math_cast");
                    else if (a->getType()->isFloatTy())
                        a = builder_->CreateFPExt(a, f64Ty, "math_ext");
                }
                auto callee = declareBuiltin("lucis_" + calleeName, f64Ty,
                    {f64Ty, f64Ty});
                auto* ret = builder_->CreateCall(callee, args, calleeName);
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::math — lerp(a, b, t) → float64 ────────────────────────
            if (calleeName == "lerp") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 3)) return {};
                for (auto*& a : args) {
                    if (a->getType()->isIntegerTy())
                        a = builder_->CreateSIToFP(a, f64Ty, "math_cast");
                    else if (a->getType()->isFloatTy())
                        a = builder_->CreateFPExt(a, f64Ty, "math_ext");
                }
                auto callee = declareBuiltin("lucis_lerp", f64Ty,
                    {f64Ty, f64Ty, f64Ty});
                auto* ret = builder_->CreateCall(callee, args, "lerp");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::math — map(val, inMin, inMax, outMin, outMax) → float64
            if (calleeName == "map") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 5)) return {};
                for (auto*& a : args) {
                    if (a->getType()->isIntegerTy())
                        a = builder_->CreateSIToFP(a, f64Ty, "math_cast");
                    else if (a->getType()->isFloatTy())
                        a = builder_->CreateFPExt(a, f64Ty, "math_ext");
                }
                auto callee = declareBuiltin("lucis_map", f64Ty,
                    {f64Ty, f64Ty, f64Ty, f64Ty, f64Ty});
                auto* ret = builder_->CreateCall(callee, args, "map");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::math — checks: float64 → bool ─────────────────────────
            static const std::unordered_set<std::string> mathChecks = {
                "isNaN", "isInf", "isFinite"
            };
            if (mathChecks.count(calleeName)) {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* arg = args[0];
                if (arg->getType()->isIntegerTy())
                    arg = builder_->CreateSIToFP(arg, f64Ty, "math_cast");
                else if (arg->getType()->isFloatTy())
                    arg = builder_->CreateFPExt(arg, f64Ty, "math_ext");
                auto callee = declareBuiltin("lucis_" + calleeName, i32Ty,
                    {f64Ty});
                auto* ret32 = builder_->CreateCall(callee, {arg},
                    calleeName + "_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), calleeName);
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::math — polymorphic: abs(T) → T ────────────────────────
            if (calleeName == "abs") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* arg = args[0];
                auto* argType = arg->getType();

                // Detect type info via variable lookup
                const TypeInfo* argTI = nullptr;
                auto argExprs = ctx->argList()->expression();
                if (auto* argIdent = dynamic_cast<LucisParser::IdentExprContext*>(
                        argExprs[0])) {
                    auto varIt = locals_.find(argIdent->IDENTIFIER()->getText());
                    if (varIt != locals_.end())
                        argTI = varIt->second.typeInfo;
                }

                std::string suffix;
                if (argTI)                            suffix = argTI->builtinSuffix;
                else if (argType->isFloatTy())        suffix = "f32";
                else if (argType->isDoubleTy())       suffix = "f64";
                else if (argType->isIntegerTy(32))    suffix = "i32";
                else if (argType->isIntegerTy(64))    suffix = "i64";
                else { suffix = "i64"; arg = builder_->CreateIntCast(arg, i64Ty, true); argType = i64Ty; }

                // Cast to matching type for the C function
                llvm::Type* paramType = argType;
                if (suffix == "i32") paramType = i32Ty;
                else if (suffix == "i64") paramType = i64Ty;
                else if (suffix == "f32") paramType = llvm::Type::getFloatTy(*context_);
                else if (suffix == "f64") paramType = f64Ty;

                if (arg->getType() != paramType) {
                    if (paramType->isIntegerTy())
                        arg = builder_->CreateIntCast(arg, paramType, true);
                    else if (paramType->isFloatingPointTy() && arg->getType()->isIntegerTy())
                        arg = builder_->CreateSIToFP(arg, paramType);
                }

                auto callee = declareBuiltin("lucis_abs_" + suffix,
                    paramType, {paramType});
                auto* ret = builder_->CreateCall(callee, {arg}, "abs");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::math — polymorphic: min/max(T, T) → T ─────────────────
            if (calleeName == "min" || calleeName == "max") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                auto* a = args[0];
                auto* b = args[1];
                auto* argType = a->getType();

                const TypeInfo* argTI = nullptr;
                auto argExprs = ctx->argList()->expression();
                if (auto* argIdent = dynamic_cast<LucisParser::IdentExprContext*>(
                        argExprs[0])) {
                    auto varIt = locals_.find(argIdent->IDENTIFIER()->getText());
                    if (varIt != locals_.end())
                        argTI = varIt->second.typeInfo;
                }

                std::string suffix;
                if (argTI)                            suffix = argTI->builtinSuffix;
                else if (argType->isFloatTy())        suffix = "f32";
                else if (argType->isDoubleTy())       suffix = "f64";
                else if (argType->isIntegerTy(32))    suffix = "i32";
                else if (argType->isIntegerTy(64))    suffix = "i64";
                else { suffix = "i64"; a = builder_->CreateIntCast(a, i64Ty, true); b = builder_->CreateIntCast(b, i64Ty, true); argType = i64Ty; }

                llvm::Type* paramType = argType;
                if (suffix == "i32") paramType = i32Ty;
                else if (suffix == "i64") paramType = i64Ty;
                else if (suffix == "u32") paramType = i32Ty;
                else if (suffix == "u64") paramType = i64Ty;
                else if (suffix == "f32") paramType = llvm::Type::getFloatTy(*context_);
                else if (suffix == "f64") paramType = f64Ty;

                if (a->getType() != paramType)
                    a = builder_->CreateIntCast(a, paramType, suffix[0] != 'u');
                if (b->getType() != paramType)
                    b = builder_->CreateIntCast(b, paramType, suffix[0] != 'u');

                auto callee = declareBuiltin("lucis_" + calleeName + "_" + suffix,
                    paramType, {paramType, paramType});
                auto* ret = builder_->CreateCall(callee, {a, b}, calleeName);
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::math — polymorphic: clamp(T, T, T) → T ────────────────
            if (calleeName == "clamp") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 3)) return {};
                auto* val = args[0];
                auto* lo  = args[1];
                auto* hi  = args[2];
                auto* argType = val->getType();

                const TypeInfo* argTI = nullptr;
                auto argExprs = ctx->argList()->expression();
                if (auto* argIdent = dynamic_cast<LucisParser::IdentExprContext*>(
                        argExprs[0])) {
                    auto varIt = locals_.find(argIdent->IDENTIFIER()->getText());
                    if (varIt != locals_.end())
                        argTI = varIt->second.typeInfo;
                }

                std::string suffix;
                if (argTI)                            suffix = argTI->builtinSuffix;
                else if (argType->isFloatTy())        suffix = "f32";
                else if (argType->isDoubleTy())       suffix = "f64";
                else if (argType->isIntegerTy(32))    suffix = "i32";
                else if (argType->isIntegerTy(64))    suffix = "i64";
                else { suffix = "i64"; val = builder_->CreateIntCast(val, i64Ty, true); lo = builder_->CreateIntCast(lo, i64Ty, true); hi = builder_->CreateIntCast(hi, i64Ty, true); argType = i64Ty; }

                llvm::Type* paramType = argType;
                if (suffix == "i32") paramType = i32Ty;
                else if (suffix == "i64") paramType = i64Ty;
                else if (suffix == "u32") paramType = i32Ty;
                else if (suffix == "u64") paramType = i64Ty;
                else if (suffix == "f32") paramType = llvm::Type::getFloatTy(*context_);
                else if (suffix == "f64") paramType = f64Ty;

                if (val->getType() != paramType)
                    val = builder_->CreateIntCast(val, paramType, suffix[0] != 'u');
                if (lo->getType() != paramType)
                    lo = builder_->CreateIntCast(lo, paramType, suffix[0] != 'u');
                if (hi->getType() != paramType)
                    hi = builder_->CreateIntCast(hi, paramType, suffix[0] != 'u');

                auto callee = declareBuiltin("lucis_clamp_" + suffix,
                    paramType, {paramType, paramType, paramType});
                auto* ret = builder_->CreateCall(callee, {val, lo, hi}, "clamp");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::string — search & match: (string, string) → bool ──────
            static const std::unordered_set<std::string> strBoolSearch = {
                "contains", "startsWith", "endsWith"
            };
            if (strBoolSearch.count(calleeName)) {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                auto callee = declareBuiltin("lucis_" + calleeName, i32Ty,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                auto* ret32 = builder_->CreateCall(callee, callArgs,
                    calleeName + "_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), calleeName);
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::string — search: (string, string) → int64 ─────────────
            if (calleeName == "indexOf" || calleeName == "lastIndexOf") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                auto callee = declareBuiltin("lucis_" + calleeName, i64Ty,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, calleeName);
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::string — count: (string, string) → usize ──────────────
            if (calleeName == "count") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                auto callee = declareBuiltin("lucis_count", usizeTy,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "count");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::string — transform: (string) → string ─────────────────
            static const std::unordered_set<std::string> strUnaryTransform = {
                "toUpper", "toLower", "trim", "trimLeft", "trimRight", "reverse"
            };
            if (strUnaryTransform.count(calleeName)) {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                // If the argument is a char (i8), skip string dispatch
                // and let the std::ascii handler below handle it
                if (!args.empty() && args[0]->getType()->isIntegerTy(8) &&
                    (calleeName == "toUpper" || calleeName == "toLower")) {
                    // Fall through to std::ascii dispatch below
                } else {
                    std::vector<llvm::Value*> callArgs;
                    extractStringArg(args[0], callArgs);
                    auto callee = declareBuiltin("lucis_" + calleeName, strTy,
                        {ptrTy, usizeTy});
                    auto* ret = builder_->CreateCall(callee, callArgs, calleeName);
                    return static_cast<llvm::Value*>(buildString(ret));
                }
            }

            // ── std::string — replace/replaceFirst: (string, string, string) → string
            if (calleeName == "replace" || calleeName == "replaceFirst") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 3)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                extractStringArg(args[2], callArgs);
                auto callee = declareBuiltin("lucis_" + calleeName, strTy,
                    {ptrTy, usizeTy, ptrTy, usizeTy, ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, calleeName);
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::string — repeat: (string, usize) → string ─────────────
            if (calleeName == "repeat") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto* n = args[1];
                if (n->getType() != usizeTy)
                    n = builder_->CreateIntCast(n, usizeTy, false);
                callArgs.push_back(n);
                auto callee = declareBuiltin("lucis_repeat", strTy,
                    {ptrTy, usizeTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "repeat");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::string — padLeft/padRight: (string, usize, char) → string
            if (calleeName == "padLeft" || calleeName == "padRight") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 3)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto* width = args[1];
                if (width->getType() != usizeTy)
                    width = builder_->CreateIntCast(width, usizeTy, false);
                callArgs.push_back(width);
                auto* fill = args[2];
                if (fill->getType() != i8Ty)
                    fill = builder_->CreateIntCast(fill, i8Ty, false);
                callArgs.push_back(fill);
                auto callee = declareBuiltin("lucis_" + calleeName, strTy,
                    {ptrTy, usizeTy, usizeTy, i8Ty});
                auto* ret = builder_->CreateCall(callee, callArgs, calleeName);
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::string — substring: (string, usize, usize) → string ───
            if (calleeName == "substring") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 3)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto* start = args[1];
                if (start->getType() != usizeTy)
                    start = builder_->CreateIntCast(start, usizeTy, false);
                callArgs.push_back(start);
                auto* length = args[2];
                if (length->getType() != usizeTy)
                    length = builder_->CreateIntCast(length, usizeTy, false);
                callArgs.push_back(length);
                auto callee = declareBuiltin("lucis_substring", strTy,
                    {ptrTy, usizeTy, usizeTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "substring");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::string — charAt: (string, usize) → char ───────────────
            if (calleeName == "charAt") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto* idx = args[1];
                if (idx->getType() != usizeTy)
                    idx = builder_->CreateIntCast(idx, usizeTy, false);
                callArgs.push_back(idx);
                auto callee = declareBuiltin("lucis_charAt", i8Ty,
                    {ptrTy, usizeTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "charAt");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::string — slice: (string, int64, int64) → string ────────
            if (calleeName == "slice") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 3)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto* start = args[1];
                if (start->getType() != i64Ty)
                    start = builder_->CreateIntCast(start, i64Ty, true);
                callArgs.push_back(start);
                auto* end = args[2];
                if (end->getType() != i64Ty)
                    end = builder_->CreateIntCast(end, i64Ty, true);
                callArgs.push_back(end);
                auto callee = declareBuiltin("lucis_slice", strTy,
                    {ptrTy, usizeTy, i64Ty, i64Ty});
                auto* ret = builder_->CreateCall(callee, callArgs, "slice");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::string — parseInt: (string) → int64 ───────────────────
            if (calleeName == "parseInt") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_parseInt", i64Ty,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "parseInt");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::string — parseIntRadix: (string, uint32) → int64 ──────
            if (calleeName == "parseIntRadix") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto* radix = args[1];
                if (radix->getType() != i32Ty)
                    radix = builder_->CreateIntCast(radix, i32Ty, false);
                callArgs.push_back(radix);
                auto callee = declareBuiltin("lucis_parseIntRadix", i64Ty,
                    {ptrTy, usizeTy, i32Ty});
                auto* ret = builder_->CreateCall(callee, callArgs, "parseIntRadix");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::string — parseFloat: (string) → float64 ───────────────
            if (calleeName == "parseFloat") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_parseFloat", f64Ty,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "parseFloat");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::string — fromCharCode: (int32) → char ─────────────────
            if (calleeName == "fromCharCode") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* code = args[0];
                if (code->getType() != i32Ty)
                    code = builder_->CreateIntCast(code, i32Ty, true);
                auto callee = declareBuiltin("lucis_fromCharCode", i8Ty,
                    {i32Ty});
                auto* ret = builder_->CreateCall(callee, {code}, "fromCharCode");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::mem — alloc: (usize) → *void ──────────────────────────
            if (calleeName == "alloc") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* size = args[0];
                if (size->getType() != usizeTy)
                    size = builder_->CreateIntCast(size, usizeTy, false);
                auto callee = declareBuiltin("lucis_alloc", ptrTy, {usizeTy});
                auto* ret = builder_->CreateCall(callee, {size}, "alloc");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::mem — allocZeroed: (usize) → *void ────────────────────
            if (calleeName == "allocZeroed") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* size = args[0];
                if (size->getType() != usizeTy)
                    size = builder_->CreateIntCast(size, usizeTy, false);
                auto callee = declareBuiltin("lucis_allocZeroed", ptrTy, {usizeTy});
                auto* ret = builder_->CreateCall(callee, {size}, "allocZeroed");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::mem — realloc: (*void, usize) → *void ─────────────────
            if (calleeName == "realloc") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                auto* ptr  = args[0];
                auto* size = args[1];
                if (size->getType() != usizeTy)
                    size = builder_->CreateIntCast(size, usizeTy, false);
                auto callee = declareBuiltin("lucis_realloc", ptrTy,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, {ptr, size}, "realloc");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::mem — compare: (*void, *void, usize) → int32 ──────────
            if (calleeName == "compare") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 3)) return {};
                auto* a = args[0];
                auto* b = args[1];
                auto* n = args[2];
                if (n->getType() != usizeTy)
                    n = builder_->CreateIntCast(n, usizeTy, false);
                auto callee = declareBuiltin("lucis_compare", i32Ty,
                    {ptrTy, ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, {a, b, n}, "compare");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::random — randInt: () → int64 ──────────────────────────
            if (calleeName == "randInt") {
                auto callee = declareBuiltin("lucis_randInt", i64Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "randInt");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::random — randIntRange: (int64, int64) → int64 ─────────
            if (calleeName == "randIntRange") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                auto* lo = args[0];
                auto* hi = args[1];
                if (lo->getType() != i64Ty)
                    lo = builder_->CreateIntCast(lo, i64Ty, true);
                if (hi->getType() != i64Ty)
                    hi = builder_->CreateIntCast(hi, i64Ty, true);
                auto callee = declareBuiltin("lucis_randIntRange", i64Ty,
                    {i64Ty, i64Ty});
                auto* ret = builder_->CreateCall(callee, {lo, hi}, "randIntRange");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::random — randUint: () → uint64 ────────────────────────
            if (calleeName == "randUint") {
                auto callee = declareBuiltin("lucis_randUint", i64Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "randUint");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::random — randFloat: () → float64 ──────────────────────
            if (calleeName == "randFloat") {
                auto callee = declareBuiltin("lucis_randFloat", f64Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "randFloat");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::random — randFloatRange: (float64, float64) → float64 ─
            if (calleeName == "randFloatRange") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                auto* lo = args[0];
                auto* hi = args[1];
                if (lo->getType() != f64Ty)
                    lo = builder_->CreateFPCast(lo, f64Ty);
                if (hi->getType() != f64Ty)
                    hi = builder_->CreateFPCast(hi, f64Ty);
                auto callee = declareBuiltin("lucis_randFloatRange", f64Ty,
                    {f64Ty, f64Ty});
                auto* ret = builder_->CreateCall(callee, {lo, hi}, "randFloatRange");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::random — randBool: () → bool ──────────────────────────
            if (calleeName == "randBool") {
                auto callee = declareBuiltin("lucis_randBool", i32Ty, {});
                auto* ret32 = builder_->CreateCall(callee, {}, "randBool_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), "randBool");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::random — randChar: () → char ──────────────────────────
            if (calleeName == "randChar") {
                auto callee = declareBuiltin("lucis_randChar", i8Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "randChar");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::random — uuid_v4: () → string ─────────────────────────
            if (calleeName == "uuid_v4") {
                auto callee = declareBuiltin("lucis_uuid_v4", strTy, {});
                auto* ret = builder_->CreateCall(callee, {}, "uuid_v4");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::time — no-arg, returns uint64 ─────────────────────────
            if (calleeName == "now") {
                auto callee = declareBuiltin("lucis_now", i64Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "now");
                return static_cast<llvm::Value*>(ret);
            }
            if (calleeName == "nowNanos") {
                auto callee = declareBuiltin("lucis_nowNanos", i64Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "nowNanos");
                return static_cast<llvm::Value*>(ret);
            }
            if (calleeName == "nowMicros") {
                auto callee = declareBuiltin("lucis_nowMicros", i64Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "nowMicros");
                return static_cast<llvm::Value*>(ret);
            }
            if (calleeName == "clock") {
                auto callee = declareBuiltin("lucis_clock", i64Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "clock");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::time — no-arg, returns int32 ──────────────────────────
            static const std::unordered_set<std::string> timeI32Funcs = {
                "year", "month", "day", "hour", "minute", "second", "weekday"
            };
            if (timeI32Funcs.count(calleeName)) {
                auto callee = declareBuiltin("lucis_" + calleeName, i32Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, calleeName);
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::time — timestamp: () → string ─────────────────────────
            if (calleeName == "timestamp") {
                auto callee = declareBuiltin("lucis_timestamp", strTy, {});
                auto* ret = builder_->CreateCall(callee, {}, "timestamp");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::time — elapsed: (uint64) → uint64 ─────────────────────
            if (calleeName == "elapsed") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                auto* since = args[0];
                if (since->getType() != i64Ty)
                    since = builder_->CreateIntCast(since, i64Ty, false);
                auto callee = declareBuiltin("lucis_elapsed", i64Ty, {i64Ty});
                auto* ret = builder_->CreateCall(callee, {since}, "elapsed");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::time — formatTime: (uint64, string) → string ──────────
            if (calleeName == "formatTime") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                auto* ms = args[0];
                if (ms->getType() != i64Ty)
                    ms = builder_->CreateIntCast(ms, i64Ty, false);
                auto* fmtPtr = builder_->CreateExtractValue(args[1], 0, "fmt_ptr");
                auto* fmtLen = builder_->CreateExtractValue(args[1], 1, "fmt_len");
                auto callee = declareBuiltin("lucis_formatTime", strTy,
                    {i64Ty, ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee,
                    {ms, fmtPtr, fmtLen}, "formatTime");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::time — parseTime: (string, string) → uint64 ───────────
            if (calleeName == "parseTime") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                auto* strPtr = builder_->CreateExtractValue(args[0], 0, "str_ptr");
                auto* strLen = builder_->CreateExtractValue(args[0], 1, "str_len");
                auto* fmtPtr = builder_->CreateExtractValue(args[1], 0, "fmt_ptr");
                auto* fmtLen = builder_->CreateExtractValue(args[1], 1, "fmt_len");
                auto callee = declareBuiltin("lucis_parseTime", i64Ty,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee,
                    {strPtr, strLen, fmtPtr, fmtLen}, "parseTime");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::fs — no-arg string returns: cwd, tempDir ──────────────
            if (calleeName == "cwd" || calleeName == "tempDir") {
                auto callee = declareBuiltin("lucis_" + calleeName, strTy, {});
                auto* ret = builder_->CreateCall(callee, {}, calleeName);
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::fs — readFile: (string) → string ──────────────────────
            if (calleeName == "readFile") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_readFile", strTy,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "readFile");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::fs — single-string-arg bool returns ────────────────────
            {
                static const std::unordered_set<std::string> fsBoolFuncs1 = {
                    "exists", "isFile", "isDir",
                    "remove", "removeDir",
                    "mkdir", "mkdirAll", "setCwd"
                };
                if (fsBoolFuncs1.count(calleeName)) {
                    std::vector<llvm::Value*> args;
                    if (auto* argList = ctx->argList()) {
                        for (auto* exprCtx : argList->expression())
                            args.push_back(castValue(visit(exprCtx)));
                    }
                    if (!requireArgs(calleeName, args, 1)) return {};
                    std::vector<llvm::Value*> callArgs;
                    extractStringArg(args[0], callArgs);
                    std::string cName = "lucis_" + calleeName;
                    auto callee = declareBuiltin(cName, i32Ty,
                        {ptrTy, usizeTy});
                    auto* ret32 = builder_->CreateCall(callee, callArgs, calleeName + "_i32");
                    auto* ret = builder_->CreateICmpNE(ret32,
                        llvm::ConstantInt::get(i32Ty, 0), calleeName);
                    return static_cast<llvm::Value*>(ret);
                }
            }

            // ── std::fs — rename: (string, string) → bool ──────────────────
            if (calleeName == "rename") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 2)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                auto callee = declareBuiltin("lucis_fsRename", i32Ty,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                auto* ret32 = builder_->CreateCall(callee, callArgs, "rename_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), "rename");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::fs — fileSize: (string) → int64 ───────────────────────
            if (calleeName == "fileSize") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_fileSize", i64Ty,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "fileSize");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::process — no-arg string returns ────────────────────────
            {
                static const std::unordered_set<std::string> procStrNoArg = {
                    "platform", "arch", "homeDir", "executablePath"
                };
                if (procStrNoArg.count(calleeName)) {
                    auto callee = declareBuiltin("lucis_" + calleeName, strTy, {});
                    auto* ret = builder_->CreateCall(callee, {}, calleeName);
                    return static_cast<llvm::Value*>(buildString(ret));
                }
            }

            // ── std::process — env: (string) → string ──────────────────────
            if (calleeName == "env") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_env", strTy,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "env");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::process — execOutput: (string) → string ───────────────
            if (calleeName == "execOutput") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_execOutput", strTy,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "execOutput");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::process — hasEnv: (string) → bool ─────────────────────
            if (calleeName == "hasEnv") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_hasEnv", i32Ty,
                    {ptrTy, usizeTy});
                auto* ret32 = builder_->CreateCall(callee, callArgs, "hasEnv_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), "hasEnv");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::process — exec: (string) → int32 ──────────────────────
            if (calleeName == "exec") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    for (auto* exprCtx : argList->expression())
                        args.push_back(castValue(visit(exprCtx)));
                }
                if (!requireArgs(calleeName, args, 1)) return {};
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_exec", i32Ty,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "exec");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::process — pid: () → int32 ─────────────────────────────
            if (calleeName == "pid") {
                auto callee = declareBuiltin("lucis_pid", i32Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "pid");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::conv — itoa: (int64) → string ─────────────────────────
            if (calleeName == "itoa") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                if (a->getType() != i64Ty)
                    a = builder_->CreateIntCast(a, i64Ty, true);
                auto callee = declareBuiltin("lucis_itoa", strTy, {i64Ty});
                auto* ret = builder_->CreateCall(callee, {a}, "itoa");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::conv — itoaRadix: (int64, uint32) → string ────────────
            if (calleeName == "itoaRadix") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* val = args[0];
                if (val->getType() != i64Ty)
                    val = builder_->CreateIntCast(val, i64Ty, true);
                auto* radix = args[1];
                if (radix->getType() != i32Ty)
                    radix = builder_->CreateIntCast(radix, i32Ty, false);
                auto callee = declareBuiltin("lucis_itoaRadix", strTy, {i64Ty, i32Ty});
                auto* ret = builder_->CreateCall(callee, {val, radix}, "itoaRadix");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::conv — utoa: (uint64) → string ────────────────────────
            if (calleeName == "utoa") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                if (a->getType() != i64Ty)
                    a = builder_->CreateIntCast(a, i64Ty, false);
                auto callee = declareBuiltin("lucis_utoa", strTy, {i64Ty});
                auto* ret = builder_->CreateCall(callee, {a}, "utoa");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::conv — ftoa: (float64) → string ───────────────────────
            if (calleeName == "ftoa") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                if (a->getType() != f64Ty)
                    a = builder_->CreateFPExt(a, f64Ty);
                auto callee = declareBuiltin("lucis_ftoa", strTy, {f64Ty});
                auto* ret = builder_->CreateCall(callee, {a}, "ftoa");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::conv — ftoaPrecision: (float64, uint32) → string ──────
            if (calleeName == "ftoaPrecision") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* val = args[0];
                if (val->getType() != f64Ty)
                    val = builder_->CreateFPExt(val, f64Ty);
                auto* prec = args[1];
                if (prec->getType() != i32Ty)
                    prec = builder_->CreateIntCast(prec, i32Ty, false);
                auto callee = declareBuiltin("lucis_ftoaPrecision", strTy, {f64Ty, i32Ty});
                auto* ret = builder_->CreateCall(callee, {val, prec}, "ftoaPrecision");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::conv — toHex/toOctal/toBinary: (uint64) → string ──────
            if (calleeName == "toHex" || calleeName == "toOctal" ||
                calleeName == "toBinary") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                if (a->getType() != i64Ty)
                    a = builder_->CreateIntCast(a, i64Ty, false);
                auto callee = declareBuiltin("lucis_" + calleeName, strTy, {i64Ty});
                auto* ret = builder_->CreateCall(callee, {a}, calleeName);
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::conv — atoi: (string) → int64 ─────────────────────────
            if (calleeName == "atoi") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_atoi", i64Ty,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "atoi");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::conv — atof: (string) → float64 ───────────────────────
            if (calleeName == "atof") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_atof", f64Ty,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "atof");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::conv — fromHex: (string) → uint64 ─────────────────────
            if (calleeName == "fromHex") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_fromHex", i64Ty,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "fromHex");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::conv — charToInt: (char) → int32 ──────────────────────
            if (calleeName == "charToInt") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto callee = declareBuiltin("lucis_charToInt", i32Ty, {i8Ty});
                auto* ret = builder_->CreateCall(callee, {args[0]}, "charToInt");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::conv — intToChar: (int32) → char ──────────────────────
            if (calleeName == "intToChar") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                if (a->getType() != i32Ty)
                    a = builder_->CreateIntCast(a, i32Ty, true);
                auto callee = declareBuiltin("lucis_intToChar", i8Ty, {i32Ty});
                auto* ret = builder_->CreateCall(callee, {a}, "intToChar");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::hash — hashString: (string) → uint64 ─────────────────
            if (calleeName == "hashString") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_hashString", i64Ty, {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "hashString");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::hash — hashInt: (int64) → uint64 ─────────────────────
            if (calleeName == "hashInt") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                if (a->getType() != i64Ty)
                    a = builder_->CreateIntCast(a, i64Ty, true);
                auto callee = declareBuiltin("lucis_hashInt", i64Ty, {i64Ty});
                auto* ret = builder_->CreateCall(callee, {a}, "hashInt");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::hash — hashCombine: (uint64, uint64) → uint64 ────────
            if (calleeName == "hashCombine") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                auto* b = args[1];
                if (a->getType() != i64Ty)
                    a = builder_->CreateIntCast(a, i64Ty, false);
                if (b->getType() != i64Ty)
                    b = builder_->CreateIntCast(b, i64Ty, false);
                auto callee = declareBuiltin("lucis_hashCombine", i64Ty, {i64Ty, i64Ty});
                auto* ret = builder_->CreateCall(callee, {a, b}, "hashCombine");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::bits — (uint64) → uint32: popcount, ctz, clz ────────
            if (calleeName == "popcount" || calleeName == "ctz" ||
                calleeName == "clz") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                if (a->getType() != i64Ty)
                    a = builder_->CreateIntCast(a, i64Ty, false);
                auto callee = declareBuiltin("lucis_" + calleeName, i32Ty, {i64Ty});
                auto* ret = builder_->CreateCall(callee, {a}, calleeName);
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::bits — (uint64, uint32) → uint64: rotl, rotr,
            //    setBit, clearBit, toggleBit ───────────────────────────────
            if (calleeName == "rotl" || calleeName == "rotr" ||
                calleeName == "setBit" || calleeName == "clearBit" ||
                calleeName == "toggleBit") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                auto* b = args[1];
                if (a->getType() != i64Ty)
                    a = builder_->CreateIntCast(a, i64Ty, false);
                if (b->getType() != i32Ty)
                    b = builder_->CreateIntCast(b, i32Ty, false);
                auto callee = declareBuiltin("lucis_" + calleeName, i64Ty, {i64Ty, i32Ty});
                auto* ret = builder_->CreateCall(callee, {a, b}, calleeName);
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::bits — (uint64) → uint64: bswap, nextPow2,
            //    bitReverse ────────────────────────────────────────────────
            if (calleeName == "bswap" || calleeName == "nextPow2" ||
                calleeName == "bitReverse") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                if (a->getType() != i64Ty)
                    a = builder_->CreateIntCast(a, i64Ty, false);
                auto callee = declareBuiltin("lucis_" + calleeName, i64Ty, {i64Ty});
                auto* ret = builder_->CreateCall(callee, {a}, calleeName);
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::bits — isPow2: (uint64) → bool ──────────────────────
            if (calleeName == "isPow2") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                if (a->getType() != i64Ty)
                    a = builder_->CreateIntCast(a, i64Ty, false);
                auto callee = declareBuiltin("lucis_isPow2", i32Ty, {i64Ty});
                auto* raw = builder_->CreateCall(callee, {a}, "isPow2_raw");
                auto* ret = builder_->CreateICmpNE(raw,
                    llvm::ConstantInt::get(i32Ty, 0), "isPow2");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::bits — testBit: (uint64, uint32) → bool ─────────────
            if (calleeName == "testBit") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                auto* b = args[1];
                if (a->getType() != i64Ty)
                    a = builder_->CreateIntCast(a, i64Ty, false);
                if (b->getType() != i32Ty)
                    b = builder_->CreateIntCast(b, i32Ty, false);
                auto callee = declareBuiltin("lucis_testBit", i32Ty, {i64Ty, i32Ty});
                auto* raw = builder_->CreateCall(callee, {a, b}, "testBit_raw");
                auto* ret = builder_->CreateICmpNE(raw,
                    llvm::ConstantInt::get(i32Ty, 0), "testBit");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::bits — extractBits: (uint64, uint32, uint32) → uint64
            if (calleeName == "extractBits") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                auto* b = args[1];
                auto* c = args[2];
                if (a->getType() != i64Ty)
                    a = builder_->CreateIntCast(a, i64Ty, false);
                if (b->getType() != i32Ty)
                    b = builder_->CreateIntCast(b, i32Ty, false);
                if (c->getType() != i32Ty)
                    c = builder_->CreateIntCast(c, i32Ty, false);
                auto callee = declareBuiltin("lucis_extractBits", i64Ty,
                    {i64Ty, i32Ty, i32Ty});
                auto* ret = builder_->CreateCall(callee, {a, b, c}, "extractBits");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::char — (char) → bool classification ─────────────────
            if (calleeName == "isAlpha" || calleeName == "isDigit" ||
                calleeName == "isAlphaNum" || calleeName == "isUpper" ||
                calleeName == "isLower" || calleeName == "isWhitespace" ||
                calleeName == "isPrintable" || calleeName == "isControl" ||
                calleeName == "isHexDigit" || calleeName == "isAscii") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                auto* i8Ty = llvm::Type::getInt8Ty(*context_);
                if (a->getType() != i8Ty)
                    a = builder_->CreateIntCast(a, i8Ty, false);
                auto callee = declareBuiltin("lucis_" + calleeName, i32Ty, {i8Ty});
                auto* raw = builder_->CreateCall(callee, {a}, calleeName + "_raw");
                auto* ret = builder_->CreateICmpNE(raw,
                    llvm::ConstantInt::get(i32Ty, 0), calleeName);
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::char — toUpper/toLower: (char) → char ───────────────
            if (calleeName == "toUpper" || calleeName == "toLower") {
                // Check if arg is char (not string) — std::char version
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                auto* i8Ty = llvm::Type::getInt8Ty(*context_);
                if (a->getType() == i8Ty ||
                    (a->getType()->isIntegerTy() &&
                     a->getType()->getIntegerBitWidth() == 8)) {
                    if (a->getType() != i8Ty)
                        a = builder_->CreateIntCast(a, i8Ty, false);
                    auto callee = declareBuiltin(
                        "lucis_char_" + calleeName, i8Ty, {i8Ty});
                    auto* ret = builder_->CreateCall(callee, {a}, calleeName);
                    return static_cast<llvm::Value*>(ret);
                }
                // Fall through to string version (std::str)
            }

            // ── std::char — toDigit: (char) → int32 ──────────────────────
            if (calleeName == "toDigit") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                auto* i8Ty = llvm::Type::getInt8Ty(*context_);
                if (a->getType() != i8Ty)
                    a = builder_->CreateIntCast(a, i8Ty, false);
                auto callee = declareBuiltin("lucis_toDigit", i32Ty, {i8Ty});
                auto* ret = builder_->CreateCall(callee, {a}, "toDigit");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::char — fromDigit: (int32) → char ────────────────────
            if (calleeName == "fromDigit") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                if (a->getType() != i32Ty)
                    a = builder_->CreateIntCast(a, i32Ty, true);
                auto* i8Ty = llvm::Type::getInt8Ty(*context_);
                auto callee = declareBuiltin("lucis_fromDigit", i8Ty, {i32Ty});
                auto* ret = builder_->CreateCall(callee, {a}, "fromDigit");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::path — join: (string, string) → string ─────────────
            if (calleeName == "join") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                if (args.size() == 2 && args[0]->getType() == strTy) {
                    std::vector<llvm::Value*> callArgs;
                    extractStringArg(args[0], callArgs);
                    extractStringArg(args[1], callArgs);
                    auto callee = declareBuiltin("lucis_pathJoin", strTy,
                        {ptrTy, usizeTy, ptrTy, usizeTy});
                    auto* ret = builder_->CreateCall(callee, callArgs, "pathJoin");
                    return static_cast<llvm::Value*>(buildString(ret));
                }
            }

            // ── std::path — single-string → string: parent, fileName, stem,
            //    extension, normalize, toAbsolute ─────────────────────────
            {
                static const std::unordered_set<std::string> pathStrFuncs = {
                    "parent", "fileName", "stem", "extension",
                    "normalize", "toAbsolute"
                };
                if (pathStrFuncs.count(calleeName)) {
                    std::vector<llvm::Value*> args;
                    if (auto* argList = ctx->argList())
                        for (auto* e : argList->expression())
                            args.push_back(castValue(visit(e)));
                    std::vector<llvm::Value*> callArgs;
                    extractStringArg(args[0], callArgs);
                    auto callee = declareBuiltin("lucis_" + calleeName, strTy,
                        {ptrTy, usizeTy});
                    auto* ret = builder_->CreateCall(callee, callArgs, calleeName);
                    return static_cast<llvm::Value*>(buildString(ret));
                }
            }

            // ── std::path — single-string → bool: isAbsolute, isRelative ─
            if (calleeName == "isAbsolute" || calleeName == "isRelative") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_" + calleeName, i32Ty,
                    {ptrTy, usizeTy});
                auto* ret32 = builder_->CreateCall(callee, callArgs, calleeName + "_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), calleeName);
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::path — separator: () → char ─────────────────────────
            if (calleeName == "separator") {
                auto callee = declareBuiltin("lucis_separator", i8Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "separator");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::path — (string, string) → string: withExtension, withFileName
            if (calleeName == "withExtension" || calleeName == "withFileName") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                auto callee = declareBuiltin("lucis_" + calleeName, strTy,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, calleeName);
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::fmt — lpad/rpad/center: (string, usize, char) → string
            if (calleeName == "lpad" || calleeName == "rpad" ||
                calleeName == "center") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto* width = args[1];
                if (width->getType() != usizeTy)
                    width = builder_->CreateIntCast(width, usizeTy, false);
                callArgs.push_back(width);
                auto* fill = args[2];
                if (fill->getType() != i8Ty)
                    fill = builder_->CreateIntCast(fill, i8Ty, false);
                callArgs.push_back(fill);
                auto callee = declareBuiltin("lucis_" + calleeName, strTy,
                    {ptrTy, usizeTy, usizeTy, i8Ty});
                auto* ret = builder_->CreateCall(callee, callArgs, calleeName);
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::fmt — hex/hexUpper/oct/bin/humanBytes: (uint64) → string
            {
                static const std::unordered_map<std::string, std::string> fmtU64Funcs = {
                    {"hex", "lucis_fmtHex"}, {"hexUpper", "lucis_fmtHexUpper"},
                    {"oct", "lucis_fmtOct"}, {"bin", "lucis_fmtBin"},
                    {"humanBytes", "lucis_humanBytes"}
                };
                auto fmtIt = fmtU64Funcs.find(calleeName);
                if (fmtIt != fmtU64Funcs.end()) {
                    std::vector<llvm::Value*> args;
                    if (auto* argList = ctx->argList())
                        for (auto* e : argList->expression())
                            args.push_back(castValue(visit(e)));
                    auto* a = args[0];
                    auto* u64Ty = llvm::Type::getInt64Ty(*context_);
                    if (a->getType() != u64Ty)
                        a = builder_->CreateIntCast(a, u64Ty, false);
                    auto callee = declareBuiltin(fmtIt->second, strTy, {u64Ty});
                    auto* ret = builder_->CreateCall(callee, {a}, calleeName);
                    return static_cast<llvm::Value*>(buildString(ret));
                }
            }

            // ── std::fmt — commas: (int64) → string ─────────────────────
            if (calleeName == "commas") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = args[0];
                if (a->getType() != i64Ty)
                    a = builder_->CreateIntCast(a, i64Ty, true);
                auto callee = declareBuiltin("lucis_commas", strTy, {i64Ty});
                auto* ret = builder_->CreateCall(callee, {a}, "commas");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::fmt — fixed: (float64, uint32) → string ────────────
            if (calleeName == "fixed") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* val = args[0];
                if (val->getType() != f64Ty)
                    val = builder_->CreateSIToFP(val, f64Ty);
                auto* dec = args[1];
                auto* u32Ty = llvm::Type::getInt32Ty(*context_);
                if (dec->getType() != u32Ty)
                    dec = builder_->CreateIntCast(dec, u32Ty, false);
                auto callee = declareBuiltin("lucis_fixed", strTy,
                    {f64Ty, u32Ty});
                auto* ret = builder_->CreateCall(callee, {val, dec}, "fixed");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::fmt — scientific/percent: (float64) → string ───────
            if (calleeName == "scientific" || calleeName == "percent") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* val = args[0];
                if (val->getType() != f64Ty)
                    val = builder_->CreateSIToFP(val, f64Ty);
                auto callee = declareBuiltin("lucis_" + calleeName, strTy,
                    {f64Ty});
                auto* ret = builder_->CreateCall(callee, {val}, calleeName);
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::regex — match: (string, string) → bool ─────────────
            if (calleeName == "match") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                auto callee = declareBuiltin("lucis_regexMatch", i32Ty,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                auto* ret32 = builder_->CreateCall(callee, callArgs, "match_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), "match");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::regex — find: (string, string) → string ────────────
            if (calleeName == "find") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                auto callee = declareBuiltin("lucis_regexFind", strTy,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "find");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::regex — findIndex: (string, string) → int64 ────────
            if (calleeName == "findIndex") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                auto callee = declareBuiltin("lucis_regexFindIndex", i64Ty,
                    {ptrTy, usizeTy, ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "findIndex");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::regex — regexReplace/regexReplaceFirst: (string, string, string) → string
            if (calleeName == "regexReplace" || calleeName == "regexReplaceFirst") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                extractStringArg(args[2], callArgs);
                auto callee = declareBuiltin("lucis_" + calleeName, strTy,
                    {ptrTy, usizeTy, ptrTy, usizeTy, ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, calleeName);
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::regex — isValid: (string) → bool ───────────────────
            if (calleeName == "isValid") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_regexIsValid", i32Ty,
                    {ptrTy, usizeTy});
                auto* ret32 = builder_->CreateCall(callee, callArgs, "isValid_i32");
                auto* ret = builder_->CreateICmpNE(ret32,
                    llvm::ConstantInt::get(i32Ty, 0), "isValid");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::encoding — string → string functions ────────────────
            {
                static const std::unordered_map<std::string, std::string> encStrFuncs = {
                    {"base64EncodeStr", "lucis_base64EncodeStr"},
                    {"base64DecodeStr", "lucis_base64DecodeStr"},
                    {"urlEncode",       "lucis_urlEncode"},
                    {"urlDecode",       "lucis_urlDecode"},
                };
                auto encIt = encStrFuncs.find(calleeName);
                if (encIt != encStrFuncs.end()) {
                    std::vector<llvm::Value*> args;
                    if (auto* argList = ctx->argList())
                        for (auto* e : argList->expression())
                            args.push_back(castValue(visit(e)));
                    std::vector<llvm::Value*> callArgs;
                    extractStringArg(args[0], callArgs);
                    auto callee = declareBuiltin(encIt->second, strTy,
                        {ptrTy, usizeTy});
                    auto* ret = builder_->CreateCall(callee, callArgs, calleeName);
                    return static_cast<llvm::Value*>(buildString(ret));
                }
            }

            // ── std::crypto — string → string hash functions ─────────────
            {
                static const std::unordered_map<std::string, std::string> cryptoStrFuncs = {
                    {"md5String",    "lucis_md5String"},
                    {"sha1String",   "lucis_sha1String"},
                    {"sha256String", "lucis_sha256String"},
                    {"sha512String", "lucis_sha512String"},
                };
                auto crIt = cryptoStrFuncs.find(calleeName);
                if (crIt != cryptoStrFuncs.end()) {
                    std::vector<llvm::Value*> args;
                    if (auto* argList = ctx->argList())
                        for (auto* e : argList->expression())
                            args.push_back(castValue(visit(e)));
                    std::vector<llvm::Value*> callArgs;
                    extractStringArg(args[0], callArgs);
                    auto callee = declareBuiltin(crIt->second, strTy,
                        {ptrTy, usizeTy});
                    auto* ret = builder_->CreateCall(callee, callArgs, calleeName);
                    return static_cast<llvm::Value*>(buildString(ret));
                }
            }

            // ── std::compress — string → string (gzip/deflate) ────────
            {
                static const std::unordered_map<std::string, std::string> compressStrFuncs = {
                    {"gzipCompress",   "lucis_gzipCompress"},
                    {"gzipDecompress", "lucis_gzipDecompress"},
                    {"deflate",        "lucis_deflate"},
                    {"inflate",        "lucis_inflate"},
                };
                auto cpIt = compressStrFuncs.find(calleeName);
                if (cpIt != compressStrFuncs.end()) {
                    std::vector<llvm::Value*> args;
                    if (auto* argList = ctx->argList())
                        for (auto* e : argList->expression())
                            args.push_back(castValue(visit(e)));
                    std::vector<llvm::Value*> callArgs;
                    extractStringArg(args[0], callArgs);
                    auto callee = declareBuiltin(cpIt->second, strTy,
                        {ptrTy, usizeTy});
                    auto* ret = builder_->CreateCall(callee, callArgs, calleeName);
                    return static_cast<llvm::Value*>(buildString(ret));
                }
            }

            // ── std::compress — compressLevel: (string, int32) → string ──
            if (calleeName == "compressLevel") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto* lvl = args[1];
                if (lvl->getType() != i32Ty)
                    lvl = builder_->CreateIntCast(lvl, i32Ty, true);
                callArgs.push_back(lvl);
                auto callee = declareBuiltin("lucis_compressLevel", strTy,
                    {ptrTy, usizeTy, i32Ty});
                auto* ret = builder_->CreateCall(callee, callArgs, "compressLevel");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::net — resolve: string → string ─────────────────────
            if (calleeName == "resolve") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_netResolve", strTy,
                    {ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "resolve");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::thread — cpuCount: () → uint32 ────────────────────
            if (calleeName == "cpuCount") {
                auto* u32Ty = llvm::Type::getInt32Ty(*context_);
                auto callee = declareBuiltin("lucis_cpuCount", u32Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "cpuCount");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::thread — threadId: () → uint64 ────────────────────
            if (calleeName == "threadId") {
                auto callee = declareBuiltin("lucis_threadId", i64Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "threadId");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::net — tcpConnect/tcpListen/udpBind: (string, uint16) → int32
            if (calleeName == "tcpConnect" || calleeName == "tcpListen" ||
                calleeName == "udpBind") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                std::vector<llvm::Value*> callArgs;
                extractStringArg(args[0], callArgs);
                auto* i16Ty = llvm::Type::getInt16Ty(*context_);
                auto* port = builder_->CreateIntCast(args[1], i16Ty, false);
                callArgs.push_back(port);
                std::string cName = "lucis_" + calleeName;
                auto callee = declareBuiltin(cName, i32Ty,
                    {ptrTy, usizeTy, i16Ty});
                auto* ret = builder_->CreateCall(callee, callArgs, calleeName);
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::net — tcpAccept: (int32) → int32 ────────────────────
            if (calleeName == "tcpAccept") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* fd = builder_->CreateIntCast(args[0], i32Ty, true);
                auto callee = declareBuiltin("lucis_tcpAccept", i32Ty, {i32Ty});
                auto* ret = builder_->CreateCall(callee, {fd}, "tcpAccept");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::net — tcpSend: (int32, string) → int64 ──────────────
            if (calleeName == "tcpSend") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* fd = builder_->CreateIntCast(args[0], i32Ty, true);
                std::vector<llvm::Value*> callArgs;
                callArgs.push_back(fd);
                extractStringArg(args[1], callArgs);
                auto callee = declareBuiltin("lucis_tcpSend", i64Ty,
                    {i32Ty, ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "tcpSend");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::net — tcpRecv/udpRecvFrom: (int32, usize) → string ──
            if (calleeName == "tcpRecv" || calleeName == "udpRecvFrom") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* fd = builder_->CreateIntCast(args[0], i32Ty, true);
                auto* maxLen = args[1];
                if (maxLen->getType() != usizeTy)
                    maxLen = builder_->CreateIntCast(maxLen, usizeTy, false);
                std::string cName = "lucis_" + calleeName;
                auto callee = declareBuiltin(cName, strTy,
                    {i32Ty, usizeTy});
                auto* ret = builder_->CreateCall(callee, {fd, maxLen}, calleeName);
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::net — udpSendTo: (int32, string, string, uint16) → int64
            if (calleeName == "udpSendTo") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* fd = builder_->CreateIntCast(args[0], i32Ty, true);
                std::vector<llvm::Value*> callArgs;
                callArgs.push_back(fd);
                extractStringArg(args[1], callArgs);  // data
                extractStringArg(args[2], callArgs);  // host
                auto* i16Ty = llvm::Type::getInt16Ty(*context_);
                auto* port = builder_->CreateIntCast(args[3], i16Ty, false);
                callArgs.push_back(port);
                auto callee = declareBuiltin("lucis_udpSendTo", i64Ty,
                    {i32Ty, ptrTy, usizeTy, ptrTy, usizeTy, i16Ty});
                auto* ret = builder_->CreateCall(callee, callArgs, "udpSendTo");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::os — no-arg → int32 ─────────────────────────────────
            {
                static const std::unordered_map<std::string, std::string> osI32NoArg = {
                    {"getpid",  "lucis_osGetpid"},
                    {"getppid", "lucis_osGetppid"},
                };
                auto osIt = osI32NoArg.find(calleeName);
                if (osIt != osI32NoArg.end()) {
                    auto callee = declareBuiltin(osIt->second, i32Ty, {});
                    auto* ret = builder_->CreateCall(callee, {}, calleeName);
                    return static_cast<llvm::Value*>(ret);
                }
            }

            // ── std::os — no-arg → uint32 ────────────────────────────────
            {
                static const std::unordered_map<std::string, std::string> osU32NoArg = {
                    {"getuid", "lucis_osGetuid"},
                    {"getgid", "lucis_osGetgid"},
                };
                auto osIt = osU32NoArg.find(calleeName);
                if (osIt != osU32NoArg.end()) {
                    auto u32Ty = llvm::Type::getInt32Ty(*context_);
                    auto callee = declareBuiltin(osIt->second, u32Ty, {});
                    auto* ret = builder_->CreateCall(callee, {}, calleeName);
                    return static_cast<llvm::Value*>(ret);
                }
            }

            // ── std::os — hostname: () → string ──────────────────────────
            if (calleeName == "hostname") {
                auto callee = declareBuiltin("lucis_osHostname", strTy, {});
                auto* ret = builder_->CreateCall(callee, {}, "hostname");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::os — pageSize: () → usize ──────────────────────────
            if (calleeName == "pageSize") {
                auto callee = declareBuiltin("lucis_osPageSize", usizeTy, {});
                auto* ret = builder_->CreateCall(callee, {}, "pageSize");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::os — errno: () → int32 ─────────────────────────────
            if (calleeName == "errno") {
                auto callee = declareBuiltin("lucis_osErrno", i32Ty, {});
                auto* ret = builder_->CreateCall(callee, {}, "osErrno");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::os — strerror: (int32) → string ────────────────────
            if (calleeName == "strerror") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* code = builder_->CreateIntCast(args[0], i32Ty, true);
                auto callee = declareBuiltin("lucis_osStrerror", strTy, {i32Ty});
                auto* ret = builder_->CreateCall(callee, {code}, "strerror");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::os — kill: (int32, int32) → int32 ──────────────────
            if (calleeName == "kill") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = builder_->CreateIntCast(args[0], i32Ty, true);
                auto* b = builder_->CreateIntCast(args[1], i32Ty, true);
                auto callee = declareBuiltin("lucis_osKill", i32Ty, {i32Ty, i32Ty});
                auto* ret = builder_->CreateCall(callee, {a, b}, "kill");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::os — fd ops: (int32) → int32 ───────────────────────
            {
                static const std::unordered_map<std::string, std::string> osFdOps = {
                    {"dup",     "lucis_osDup"},
                    {"closeFd", "lucis_osCloseFd"},
                };
                auto osIt = osFdOps.find(calleeName);
                if (osIt != osFdOps.end()) {
                    std::vector<llvm::Value*> args;
                    if (auto* argList = ctx->argList())
                        for (auto* e : argList->expression())
                            args.push_back(castValue(visit(e)));
                    auto* fd = builder_->CreateIntCast(args[0], i32Ty, true);
                    auto callee = declareBuiltin(osIt->second, i32Ty, {i32Ty});
                    auto* ret = builder_->CreateCall(callee, {fd}, calleeName);
                    return static_cast<llvm::Value*>(ret);
                }
            }

            // ── std::os — dup2: (int32, int32) → int32 ──────────────────
            if (calleeName == "dup2") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* a = builder_->CreateIntCast(args[0], i32Ty, true);
                auto* b = builder_->CreateIntCast(args[1], i32Ty, true);
                auto callee = declareBuiltin("lucis_osDup2", i32Ty, {i32Ty, i32Ty});
                auto* ret = builder_->CreateCall(callee, {a, b}, "dup2");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::io — readLines: () → Vec<string> ──────────────────────
            if (calleeName == "readLines") {
                auto* vecTy    = getOrCreateVecStructType();
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "readLines_out");
                auto callee = declareBuiltin("lucis_readLines", voidTy, {ptrTy});
                builder_->CreateCall(callee, {outAlloca});
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "readLines_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::io — readNBytes: (usize) → Vec<uint8> ─────────────────
            if (calleeName == "readNBytes") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy    = getOrCreateVecStructType();
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "readNBytes_out");
                auto* n = args[0];
                if (n->getType() != usizeTy)
                    n = builder_->CreateIntCast(n, usizeTy, false);
                auto callee = declareBuiltin("lucis_readNBytes", voidTy, {ptrTy, usizeTy});
                builder_->CreateCall(callee, {outAlloca, n});
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "readNBytes_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::str — split: (string, string) → Vec<string> ───────────
            if (calleeName == "split") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy    = getOrCreateVecStructType();
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "split_out");
                std::vector<llvm::Value*> callArgs = {outAlloca};
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                auto callee = declareBuiltin("lucis_split", voidTy,
                    {ptrTy, ptrTy, usizeTy, ptrTy, usizeTy});
                builder_->CreateCall(callee, callArgs);
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "split_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::str — splitN: (string, string, usize) → Vec<string> ───
            if (calleeName == "splitN") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy    = getOrCreateVecStructType();
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "splitN_out");
                std::vector<llvm::Value*> callArgs = {outAlloca};
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                auto* n = args[2];
                if (n->getType() != usizeTy)
                    n = builder_->CreateIntCast(n, usizeTy, false);
                callArgs.push_back(n);
                auto callee = declareBuiltin("lucis_splitN", voidTy,
                    {ptrTy, ptrTy, usizeTy, ptrTy, usizeTy, usizeTy});
                builder_->CreateCall(callee, callArgs);
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "splitN_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::str — joinVec: (Vec<string>, string) → string ─────────
            if (calleeName == "joinVec") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy       = getOrCreateVecStructType();
                auto* vecArgAlloca = builder_->CreateAlloca(vecTy, nullptr, "joinVec_arg");
                builder_->CreateStore(args[0], vecArgAlloca);
                std::vector<llvm::Value*> callArgs = {vecArgAlloca};
                extractStringArg(args[1], callArgs);
                auto callee = declareBuiltin("lucis_joinVec", strTy,
                    {ptrTy, ptrTy, usizeTy});
                auto* ret = builder_->CreateCall(callee, callArgs, "joinVec");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::str — lines: (string) → Vec<string> ───────────────────
            if (calleeName == "lines") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy    = getOrCreateVecStructType();
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "lines_out");
                std::vector<llvm::Value*> callArgs = {outAlloca};
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_lines", voidTy,
                    {ptrTy, ptrTy, usizeTy});
                builder_->CreateCall(callee, callArgs);
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "lines_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::str — chars: (string) → Vec<char> ─────────────────────
            if (calleeName == "chars") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy    = getOrCreateVecStructType();
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "chars_out");
                std::vector<llvm::Value*> callArgs = {outAlloca};
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_chars", voidTy,
                    {ptrTy, ptrTy, usizeTy});
                builder_->CreateCall(callee, callArgs);
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "chars_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::str — fromChars: (Vec<char>) → string ─────────────────
            if (calleeName == "fromChars") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy       = getOrCreateVecStructType();
                auto* vecArgAlloca = builder_->CreateAlloca(vecTy, nullptr, "fromChars_arg");
                builder_->CreateStore(args[0], vecArgAlloca);
                auto callee = declareBuiltin("lucis_fromCharsVec", strTy, {ptrTy});
                auto* ret = builder_->CreateCall(callee, {vecArgAlloca}, "fromChars");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::str — toBytes: (string) → Vec<uint8> ──────────────────
            if (calleeName == "toBytes") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy    = getOrCreateVecStructType();
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "toBytes_out");
                std::vector<llvm::Value*> callArgs = {outAlloca};
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_toBytes", voidTy,
                    {ptrTy, ptrTy, usizeTy});
                builder_->CreateCall(callee, callArgs);
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "toBytes_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::str — fromBytes: (Vec<uint8>) → string ────────────────
            if (calleeName == "fromBytes") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy       = getOrCreateVecStructType();
                auto* vecArgAlloca = builder_->CreateAlloca(vecTy, nullptr, "fromBytes_arg");
                builder_->CreateStore(args[0], vecArgAlloca);
                auto callee = declareBuiltin("lucis_fromBytesVec", strTy, {ptrTy});
                auto* ret = builder_->CreateCall(callee, {vecArgAlloca}, "fromBytes");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::fs — listDir: (string) → Vec<string> ──────────────────
            if (calleeName == "listDir") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy    = getOrCreateVecStructType();
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "listDir_out");
                std::vector<llvm::Value*> callArgs = {outAlloca};
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_listDir", voidTy,
                    {ptrTy, ptrTy, usizeTy});
                builder_->CreateCall(callee, callArgs);
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "listDir_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::fs — readBytes: (string) → Vec<uint8> ─────────────────
            if (calleeName == "readBytes") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy    = getOrCreateVecStructType();
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "readBytes_out");
                std::vector<llvm::Value*> callArgs = {outAlloca};
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_readFileBytes", voidTy,
                    {ptrTy, ptrTy, usizeTy});
                builder_->CreateCall(callee, callArgs);
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "readBytes_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::hash — hashBytes: (Vec<uint8>) → uint64 ───────────────
            if (calleeName == "hashBytes") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy       = getOrCreateVecStructType();
                auto* vecArgAlloca = builder_->CreateAlloca(vecTy, nullptr, "hashBytes_arg");
                builder_->CreateStore(args[0], vecArgAlloca);
                auto callee = declareBuiltin("lucis_hashBytesVec", i64Ty, {ptrTy});
                auto* ret = builder_->CreateCall(callee, {vecArgAlloca}, "hashBytes");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::hash — crc32: (Vec<uint8>) → uint32 ───────────────────
            if (calleeName == "crc32") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy       = getOrCreateVecStructType();
                auto* vecArgAlloca = builder_->CreateAlloca(vecTy, nullptr, "crc32_arg");
                builder_->CreateStore(args[0], vecArgAlloca);
                auto callee = declareBuiltin("lucis_crc32Bytes", i32Ty, {ptrTy});
                auto* ret = builder_->CreateCall(callee, {vecArgAlloca}, "crc32");
                return static_cast<llvm::Value*>(ret);
            }

            // ── std::regex — findAll: (string, string) → Vec<string> ───────
            if (calleeName == "findAll") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy    = getOrCreateVecStructType();
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "findAll_out");
                std::vector<llvm::Value*> callArgs = {outAlloca};
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                auto callee = declareBuiltin("lucis_regexFindAll", voidTy,
                    {ptrTy, ptrTy, usizeTy, ptrTy, usizeTy});
                builder_->CreateCall(callee, callArgs);
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "findAll_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::regex — regexSplit: (string, string) → Vec<string> ────
            if (calleeName == "regexSplit") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy    = getOrCreateVecStructType();
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "regexSplit_out");
                std::vector<llvm::Value*> callArgs = {outAlloca};
                extractStringArg(args[0], callArgs);
                extractStringArg(args[1], callArgs);
                auto callee = declareBuiltin("lucis_regexSplit", voidTy,
                    {ptrTy, ptrTy, usizeTy, ptrTy, usizeTy});
                builder_->CreateCall(callee, callArgs);
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "regexSplit_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::encoding — base64Encode: (Vec<uint8>) → string ─────────
            if (calleeName == "base64Encode") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy       = getOrCreateVecStructType();
                auto* vecArgAlloca = builder_->CreateAlloca(vecTy, nullptr, "b64enc_arg");
                builder_->CreateStore(args[0], vecArgAlloca);
                auto callee = declareBuiltin("lucis_base64EncodeVec", strTy, {ptrTy});
                auto* ret = builder_->CreateCall(callee, {vecArgAlloca}, "base64Encode");
                return static_cast<llvm::Value*>(buildString(ret));
            }

            // ── std::encoding — base64Decode: (string) → Vec<uint8> ─────────
            if (calleeName == "base64Decode") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy    = getOrCreateVecStructType();
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "b64dec_out");
                std::vector<llvm::Value*> callArgs = {outAlloca};
                extractStringArg(args[0], callArgs);
                auto callee = declareBuiltin("lucis_base64DecodeVec", voidTy,
                    {ptrTy, ptrTy, usizeTy});
                builder_->CreateCall(callee, callArgs);
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "b64dec_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::crypto — md5/sha1/sha256/sha512: (Vec<uint8>) → string ─
            {
                static const std::unordered_map<std::string, std::string> cryptoByteFuncs = {
                    {"md5",    "lucis_md5Bytes"},
                    {"sha1",   "lucis_sha1Bytes"},
                    {"sha256", "lucis_sha256Bytes"},
                    {"sha512", "lucis_sha512Bytes"},
                };
                auto crIt = cryptoByteFuncs.find(calleeName);
                if (crIt != cryptoByteFuncs.end()) {
                    std::vector<llvm::Value*> args;
                    if (auto* argList = ctx->argList())
                        for (auto* e : argList->expression())
                            args.push_back(castValue(visit(e)));
                    auto* vecTy       = getOrCreateVecStructType();
                    auto* vecArgAlloca = builder_->CreateAlloca(vecTy, nullptr, calleeName + "_arg");
                    builder_->CreateStore(args[0], vecArgAlloca);
                    auto callee = declareBuiltin(crIt->second, strTy, {ptrTy});
                    auto* ret = builder_->CreateCall(callee, {vecArgAlloca}, calleeName);
                    return static_cast<llvm::Value*>(buildString(ret));
                }
            }

            // ── std::crypto — hmacSha256: (Vec<uint8>, Vec<uint8>) → Vec<uint8>
            if (calleeName == "hmacSha256") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy      = getOrCreateVecStructType();
                auto* keyAlloca  = builder_->CreateAlloca(vecTy, nullptr, "hmac_key");
                auto* dataAlloca = builder_->CreateAlloca(vecTy, nullptr, "hmac_data");
                auto* outAlloca  = builder_->CreateAlloca(vecTy, nullptr, "hmac_out");
                builder_->CreateStore(args[0], keyAlloca);
                builder_->CreateStore(args[1], dataAlloca);
                auto callee = declareBuiltin("lucis_hmacSha256", voidTy,
                    {ptrTy, ptrTy, ptrTy});
                builder_->CreateCall(callee, {outAlloca, keyAlloca, dataAlloca});
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "hmac_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::crypto — randomBytes: (usize) → Vec<uint8> ────────────
            if (calleeName == "randomBytes") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy    = getOrCreateVecStructType();
                auto* outAlloca = builder_->CreateAlloca(vecTy, nullptr, "randomBytes_out");
                auto* n = args[0];
                if (n->getType() != usizeTy)
                    n = builder_->CreateIntCast(n, usizeTy, false);
                auto callee = declareBuiltin("lucis_randomBytes", voidTy, {ptrTy, usizeTy});
                builder_->CreateCall(callee, {outAlloca, n});
                auto* result = builder_->CreateLoad(vecTy, outAlloca, "randomBytes_val");
                return static_cast<llvm::Value*>(result);
            }

            // ── std::path — joinAll: (Vec<string>) → string ─────────────────
            if (calleeName == "joinAll") {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList())
                    for (auto* e : argList->expression())
                        args.push_back(castValue(visit(e)));
                auto* vecTy       = getOrCreateVecStructType();
                auto* vecArgAlloca = builder_->CreateAlloca(vecTy, nullptr, "joinAll_arg");
                builder_->CreateStore(args[0], vecArgAlloca);
                auto callee = declareBuiltin("lucis_joinAllVec", strTy, {ptrTy});
                auto* ret = builder_->CreateCall(callee, {vecArgAlloca}, "joinAll");
                return static_cast<llvm::Value*>(buildString(ret));
            }
        }

        // Bare enum variant call via `use EnumType::*;`
        if (!calleeName.empty()) {
            auto evIt = enumVariantImports_.find(calleeName);
            if (evIt != enumVariantImports_.end()) {
                auto* enumType = evIt->second.enumType;
                auto* variantInfo = evIt->second.variantInfo;
                std::vector<llvm::Value*> payloadValues(variantInfo->payloadFields.size());
                if (auto* argList = ctx->argList()) {
                    auto exprs = argList->expression();
                    for (size_t i = 0; i < exprs.size() && i < payloadValues.size(); i++) {
                        payloadValues[i] = castValue(visit(exprs[i]));
                    }
                }
                for (auto* expr : ctx->argList() ? ctx->argList()->expression()
                                                  : std::vector<LucisParser::ExpressionContext*>{})
                    consumeExprIfOwnedLocal(expr);
                return static_cast<llvm::Value*>(
                    buildEnumVariantValue(enumType, *variantInfo, payloadValues));
            }
        }

generic_template_handled:
        if (!fnTI) {
            std::vector<const TypeInfo*> argTypes;
            if (auto* argList = ctx->argList()) {
                for (auto* exprCtx : argList->expression())
                    argTypes.push_back(resolveExprTypeInfo(exprCtx));
            }

            auto fit = genericFuncTemplates_.find(calleeName);
            if (fit != genericFuncTemplates_.end()) {
                std::vector<LucisParser::ParamContext*> formalParams;
                if (auto* paramList = fit->second.decl->paramList())
                    formalParams = paramList->param();

                auto inferred = inferGenericTypeArgs(fit->second.typeParams, formalParams, argTypes);
                if (inferred)
                    directFn = instantiateGenericFunc(calleeName, fit->second, *inferred);
            }
        }

        auto it = locals_.find(calleeName);
        if (it != locals_.end() && (it->second.typeInfo->kind == TypeKind::Function ||
            it->second.typeInfo->kind == TypeKind::Closure)) {
            fnTI = it->second.typeInfo;
        } else if (!fnTI && !directFn) {
            // Check module-level functions for direct call (with mangled name)
            auto resolvedName = resolveCallTarget(calleeName);
            directFn = module_->getFunction(resolvedName);

            // Lazy declare C function from #include bindings
            if (!directFn)
                directFn = declareCFunction(calleeName);
        }
    }

    // Direct call to a module-level function
    if (directFn) {
        auto* fnType = directFn->getFunctionType();

        // Check if this is a variadic function
        auto vit = variadicFunctions_.find(directFn->getName().str());
        if (vit != variadicFunctions_.end()) {
            auto& vfInfo = vit->second;
            size_t varIdx = vfInfo.variadicParamIdx;
            if (!vfInfo.elementType) {
                std::cerr << "lucis: internal error: variadic function missing elementType\n";
                return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
            }
            auto* elemTy = vfInfo.elementType->toLLVMType(*context_, module_->getDataLayout());

            std::vector<llvm::Value*> args;
            std::vector<llvm::Value*> variadicArgs;

            if (auto* argList = ctx->argList()) {
                for (size_t i = 0; i < argList->expression().size(); i++) {
                    auto* argVal = castValue(visit(argList->expression(i)));
                    if (i < varIdx) {
                        // Normal param: cast if needed
                        if (argVal->getType() != fnType->getParamType(i)) {
                            if (argVal->getType()->isIntegerTy() && fnType->getParamType(i)->isIntegerTy())
                                argVal = builder_->CreateIntCast(argVal, fnType->getParamType(i), true);
                            else if (argVal->getType()->isFloatingPointTy() && fnType->getParamType(i)->isFloatingPointTy()) {
                                if (argVal->getType()->getPrimitiveSizeInBits() > fnType->getParamType(i)->getPrimitiveSizeInBits())
                                    argVal = builder_->CreateFPTrunc(argVal, fnType->getParamType(i));
                                else
                                    argVal = builder_->CreateFPExt(argVal, fnType->getParamType(i));
                            }
                        }
                        args.push_back(argVal);
                    } else {
                        // Variadic arg: cast to element type if needed
                        if (argVal->getType() != elemTy) {
                            if (argVal->getType()->isIntegerTy() && elemTy->isIntegerTy())
                                argVal = builder_->CreateIntCast(argVal, elemTy, vfInfo.elementType->isSigned);
                            else if (argVal->getType()->isFloatingPointTy() && elemTy->isFloatingPointTy()) {
                                if (argVal->getType()->getPrimitiveSizeInBits() > elemTy->getPrimitiveSizeInBits())
                                    argVal = builder_->CreateFPTrunc(argVal, elemTy);
                                else
                                    argVal = builder_->CreateFPExt(argVal, elemTy);
                            }
                        }
                        variadicArgs.push_back(argVal);
                    }
                }
            }

            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
            auto* ptrTy = llvm::PointerType::getUnqual(*context_);

            if (variadicArgs.empty()) {
                // No variadic args: pass null + 0
                args.push_back(llvm::ConstantPointerNull::get(ptrTy));
                args.push_back(llvm::ConstantInt::get(i64Ty, 0));
            } else {
                // Pack variadic args into a stack-allocated array
                auto* arrTy = llvm::ArrayType::get(elemTy, variadicArgs.size());
                auto* arrAlloca = builder_->CreateAlloca(arrTy, nullptr, "variadic.pack");
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                for (size_t j = 0; j < variadicArgs.size(); j++) {
                    auto* idx = llvm::ConstantInt::get(i64Ty, j);
                    auto* gep = builder_->CreateGEP(arrTy, arrAlloca, {zero, idx});
                    builder_->CreateStore(variadicArgs[j], gep);
                }
                args.push_back(arrAlloca);
                args.push_back(llvm::ConstantInt::get(i64Ty, variadicArgs.size()));
            }

            auto* result = builder_->CreateCall(directFn, args,
                directFn->getReturnType()->isVoidTy() ? "" : "call");
            return static_cast<llvm::Value*>(result);
        }

        // ── C extern variadic function as expression (e.g. printf) ──
        if (externCFunctions_.count(directFn->getName().str()) && directFn->isVarArg()) {
            unsigned fixedParams = fnType->getNumParams();
            std::vector<llvm::Value*> args;
            if (auto* argList = ctx->argList()) {
                for (size_t i = 0; i < argList->expression().size(); i++) {
                    auto* argVal = castValue(visit(argList->expression(i)));
                    if (i < fixedParams) {
                        if (argVal->getType() != fnType->getParamType(i)) {
                            if (argVal->getType()->isIntegerTy() && fnType->getParamType(i)->isIntegerTy())
                                argVal = builder_->CreateIntCast(argVal, fnType->getParamType(i), true);
                            else if (argVal->getType()->isFloatingPointTy() && fnType->getParamType(i)->isFloatingPointTy())
                                argVal = builder_->CreateFPExt(argVal, fnType->getParamType(i));
                        }
                    } else {
                        // C default argument promotions
                        if (argVal->getType()->isFloatTy())
                            argVal = builder_->CreateFPExt(argVal, llvm::Type::getDoubleTy(*context_), "c_promote_f64");
                        else if (argVal->getType()->isIntegerTy()) {
                            unsigned bw = argVal->getType()->getIntegerBitWidth();
                            if (bw < 32)
                                argVal = builder_->CreateSExt(argVal, llvm::Type::getInt32Ty(*context_), "c_promote_i32");
                            else if (bw > 64)
                                argVal = builder_->CreateTrunc(argVal, llvm::Type::getInt32Ty(*context_), "c_trunc_i32");
                        } else if (argVal->getType()->isStructTy()) {
                            // Lucis string { ptr, i64 } → extract ptr for C variadics
                            auto* st = llvm::cast<llvm::StructType>(argVal->getType());
                            if (st->getNumElements() == 2 &&
                                st->getElementType(0)->isPointerTy() &&
                                st->getElementType(1)->isIntegerTy())
                                argVal = builder_->CreateExtractValue(argVal, 0, "str_to_cptr");
                        }
                    }
                    args.push_back(argVal);
                }
            }
            auto* result = builder_->CreateCall(directFn, args,
                directFn->getReturnType()->isVoidTy() ? "" : "call");
            return static_cast<llvm::Value*>(result);
        }

        std::vector<llvm::Value*> args;

        // Check if this C function needs ABI struct coercion
        auto abiIt = cabiInfos_.find(directFn->getName().str());
        bool needsABI = (abiIt != cabiInfos_.end());
        CABIInfo* abi = needsABI ? &abiIt->second : nullptr;

        // Sret: allocate space for large struct return, pass as hidden first arg
        llvm::AllocaInst* sretAlloca = nullptr;
        if (abi && abi->returnInfo.kind == ABIArgKind::Indirect) {
            sretAlloca = builder_->CreateAlloca(abi->originalRetType, nullptr, "sret");
            args.push_back(sretAlloca);
        }

        if (auto* argList = ctx->argList()) {
            for (size_t i = 0; i < argList->expression().size(); i++) {
                auto* argVal = castValue(visit(argList->expression(i)));

                // ABI struct coercion for C functions
                if (abi && i < abi->paramInfos.size()) {
                    auto& pi = abi->paramInfos[i];
                    if (pi.kind == ABIArgKind::Coerce && argVal->getType()->isStructTy()) {
                        // Store struct → load as coerced integer type
                        auto* tmp = builder_->CreateAlloca(argVal->getType(), nullptr, "abi.coerce");
                        builder_->CreateStore(argVal, tmp);
                        argVal = builder_->CreateLoad(pi.coercedType, tmp, "abi.coerced");
                        args.push_back(argVal);
                        continue;
                    }
                    if (pi.kind == ABIArgKind::Expand && argVal->getType()->isStructTy()) {
                        // Store struct → load as split parts (e.g., two i64 args)
                        auto* tmp = builder_->CreateAlloca(argVal->getType(), nullptr, "abi.expand");
                        builder_->CreateStore(argVal, tmp);
                        auto* i8Ty = llvm::Type::getInt8Ty(*context_);
                        uint64_t offset = 0;
                        for (auto* partTy : pi.expandedTypes) {
                            llvm::Value* partPtr = tmp;
                            if (offset > 0)
                                partPtr = builder_->CreateGEP(i8Ty, tmp,
                                    llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), offset));
                            args.push_back(builder_->CreateLoad(partTy, partPtr, "abi.part"));
                            offset += module_->getDataLayout().getTypeAllocSize(partTy);
                        }
                        continue;
                    }
                    if (pi.kind == ABIArgKind::Indirect && argVal->getType()->isStructTy()) {
                        // Store struct → pass pointer (byval)
                        auto* tmp = builder_->CreateAlloca(argVal->getType(), nullptr, "abi.byval");
                        builder_->CreateStore(argVal, tmp);
                        args.push_back(tmp);
                        continue;
                    }
                }

                // Standard type coercion (non-struct, plus array ABI lowering)
                if (i < fnType->getNumParams() && argVal->getType() != fnType->getParamType(i)) {
                    // Compute actual param index (accounting for sret offset)
                    unsigned llvmIdx = static_cast<unsigned>(i);
                    if (sretAlloca) llvmIdx++;
                    if (llvmIdx < fnType->getNumParams()) {
                        auto* expectedTy = fnType->getParamType(llvmIdx);
                        argVal = coerceArrayArg(argVal, expectedTy);
                        if (argVal->getType() != expectedTy) {
                            if (argVal->getType()->isIntegerTy() && expectedTy->isIntegerTy())
                                argVal = builder_->CreateIntCast(argVal, expectedTy, true);
                            else if (argVal->getType()->isFloatingPointTy() && expectedTy->isFloatingPointTy()) {
                                if (argVal->getType()->getPrimitiveSizeInBits() > expectedTy->getPrimitiveSizeInBits())
                                    argVal = builder_->CreateFPTrunc(argVal, expectedTy);
                                else
                                    argVal = builder_->CreateFPExt(argVal, expectedTy);
                            } else if (argVal->getType()->isStructTy() && expectedTy->isStructTy()) {
                            // Tuple/struct element-wise coercion (e.g. {i256, i256} → {i32, i32})
                            auto* srcST = llvm::cast<llvm::StructType>(argVal->getType());
                            auto* dstST = llvm::cast<llvm::StructType>(expectedTy);
                            if (srcST->getNumElements() == dstST->getNumElements()) {
                                llvm::Value* result = llvm::UndefValue::get(expectedTy);
                                for (unsigned e = 0; e < srcST->getNumElements(); e++) {
                                    auto* elem = builder_->CreateExtractValue(argVal, {e}, "tup_e");
                                    auto* srcElemTy = srcST->getElementType(e);
                                    auto* dstElemTy = dstST->getElementType(e);
                                    if (srcElemTy != dstElemTy) {
                                        if (srcElemTy->isIntegerTy() && dstElemTy->isIntegerTy())
                                            elem = builder_->CreateIntCast(elem, dstElemTy, true, "tup_cast");
                                        else if (srcElemTy->isFloatingPointTy() && dstElemTy->isFloatingPointTy()) {
                                            if (srcElemTy->getPrimitiveSizeInBits() > dstElemTy->getPrimitiveSizeInBits())
                                                elem = builder_->CreateFPTrunc(elem, dstElemTy, "tup_ftrunc");
                                            else
                                                elem = builder_->CreateFPExt(elem, dstElemTy, "tup_fext");
                                        } else if (srcElemTy->isIntegerTy() && dstElemTy->isFloatingPointTy())
                                            elem = builder_->CreateSIToFP(elem, dstElemTy, "tup_itof");
                                        else if (srcElemTy->isFloatingPointTy() && dstElemTy->isIntegerTy())
                                            elem = builder_->CreateFPToSI(elem, dstElemTy, "tup_ftoi");
                                    }
                                    result = builder_->CreateInsertValue(result, elem, {e}, "tup_ins");
                                }
                                argVal = result;
                                }
                            }
                        }
                    }
                }
                // For untyped variadic args in user-defined functions,
                // apply default C argument promotions (float -> double, small ints -> i32)
                if (directFn->isVarArg() && i >= fnType->getNumParams()) {
                    if (argVal->getType()->isFloatTy()) {
                        argVal = builder_->CreateFPExt(argVal, llvm::Type::getDoubleTy(*context_), "c_promote_f64");
                    } else if (argVal->getType()->isIntegerTy()) {
                        unsigned bw = argVal->getType()->getIntegerBitWidth();
                        if (bw < 32) {
                            if (bw == 1)
                                argVal = builder_->CreateZExt(argVal, llvm::Type::getInt32Ty(*context_), "c_promote_bool");
                            else
                                argVal = builder_->CreateSExt(argVal, llvm::Type::getInt32Ty(*context_), "c_promote_i32");
                        } else if (bw > 64) {
                            argVal = builder_->CreateTrunc(argVal, llvm::Type::getInt32Ty(*context_), "c_trunc_i32");
                        }
                    } else if (argVal->getType()->isStructTy()) {
                        auto* st = llvm::cast<llvm::StructType>(argVal->getType());
                        if (st->getNumElements() == 2 &&
                            st->getElementType(0)->isPointerTy() &&
                            st->getElementType(1)->isIntegerTy())
                            argVal = builder_->CreateExtractValue(argVal, 0, "str_to_cptr");
                    }
                }

                args.push_back(argVal);
            }
        }

        auto* callResult = builder_->CreateCall(directFn, args,
            directFn->getReturnType()->isVoidTy() ? "" : "call");

        if (auto* argList = ctx->argList()) {
            auto exprs = argList->expression();
            size_t n = std::min(exprs.size(), args.size());
            for (size_t i = 0; i < n; i++) {
                llvm::Type* expectedTy = (i < fnType->getNumParams())
                    ? fnType->getParamType(static_cast<unsigned>(i))
                    : nullptr;
                cleanupTempArg(exprs[i], args[i], expectedTy);
            }
        }

        // ABI return coercion: convert coerced return back to original struct
        if (abi) {
            if (abi->returnInfo.kind == ABIArgKind::Indirect) {
                // Sret: result is in the alloca
                auto* structVal = builder_->CreateLoad(abi->originalRetType, sretAlloca, "sret.load");
                return static_cast<llvm::Value*>(structVal);
            }
            if (abi->returnInfo.kind == ABIArgKind::Coerce && abi->originalRetType->isStructTy()) {
                // Coerced return (e.g., i64 → {i32, i32})
                auto* tmp = builder_->CreateAlloca(abi->returnInfo.coercedType, nullptr, "abi.ret");
                builder_->CreateStore(callResult, tmp);
                auto* structVal = builder_->CreateLoad(abi->originalRetType, tmp, "abi.uncoerce");
                return static_cast<llvm::Value*>(structVal);
            }
        }

        return static_cast<llvm::Value*>(callResult);
    }

    // Indirect call via function pointer or closure variable
    if (!fnTI || (fnTI->kind != TypeKind::Function && fnTI->kind != TypeKind::Closure)) {
        std::cerr << "lucis: expression is not a function type\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    // Get the function pointer value (or closure struct)
    auto* calleeVal = castValue(visit(baseExpr));

    llvm::Value* fnPtr = calleeVal;
    llvm::Value* contextPtr = nullptr;

    if (fnTI->kind == TypeKind::Closure) {
        // Closure is a struct {fn_ptr, context_ptr}
        fnPtr = builder_->CreateExtractValue(calleeVal, {0}, "closure_fn");
        contextPtr = builder_->CreateExtractValue(calleeVal, {1}, "closure_ctx");
    }

    // Build LLVM FunctionType from TypeInfo
    auto* retLLVM = fnTI->returnType->toLLVMType(*context_, module_->getDataLayout());
    std::vector<llvm::Type*> paramLLVM;
    for (size_t pi = 0; pi < fnTI->paramTypes.size(); pi++) {
        auto* pti = fnTI->paramTypes[pi];
        if (pi < fnTI->paramDims.size() && fnTI->paramDims[pi] > 0) {
            // []T param: lower to the { ptr, usize } slice ABI.
            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            paramLLVM.push_back(llvm::StructType::get(*context_, {ptrTy, usizeTy}));
        } else {
            paramLLVM.push_back(pti->toLLVMType(*context_, module_->getDataLayout()));
        }
    }
    // For closures, the first hidden param is the context pointer
    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
    if (contextPtr) {
        paramLLVM.insert(paramLLVM.begin(), ptrTy);
    }
    auto* fnType = llvm::FunctionType::get(retLLVM, paramLLVM, fnTI->isVariadic);

    // Collect arguments
    std::vector<llvm::Value*> args;
    // Prepend context pointer for closure calls
    if (contextPtr) {
        args.push_back(contextPtr);
    }
    if (auto* argList = ctx->argList()) {
        for (size_t i = 0; i < argList->expression().size(); i++) {
            auto* argVal = castValue(visit(argList->expression(i)));
            // Cast if needed (offset by 1 for closure context)
            size_t paramIdx = contextPtr ? i + 1 : i;
            if (paramIdx < paramLLVM.size() && argVal->getType() != paramLLVM[paramIdx]) {
                argVal = coerceArrayArg(argVal, paramLLVM[paramIdx]);
                if (argVal->getType() != paramLLVM[paramIdx]) {
                    if (argVal->getType()->isIntegerTy() && paramLLVM[paramIdx]->isIntegerTy())
                        argVal = builder_->CreateIntCast(argVal, paramLLVM[paramIdx],
                                                         contextPtr ? false : fnTI->paramTypes[i]->isSigned);
                    else if (argVal->getType()->isFloatingPointTy() && paramLLVM[paramIdx]->isFloatingPointTy()) {
                        if (argVal->getType()->getPrimitiveSizeInBits() > paramLLVM[paramIdx]->getPrimitiveSizeInBits())
                            argVal = builder_->CreateFPTrunc(argVal, paramLLVM[paramIdx]);
                        else
                            argVal = builder_->CreateFPExt(argVal, paramLLVM[paramIdx]);
                    }
                }
            }
            args.push_back(argVal);
        }
    }

    bool isVoidCall = fnType->getReturnType()->isVoidTy();
    auto* result = isVoidCall
        ? builder_->CreateCall(llvm::FunctionCallee(fnType, fnPtr), args)
        : builder_->CreateCall(llvm::FunctionCallee(fnType, fnPtr), args, "fncall");

    if (auto* argList = ctx->argList()) {
        auto exprs = argList->expression();
        size_t n = std::min(exprs.size(), args.size());
        for (size_t i = 0; i < n; i++) {
            llvm::Type* expectedTy = (i < paramLLVM.size()) ? paramLLVM[i] : nullptr;
            if (expectedTy) {
                cleanupTempArg(exprs[i], args[i + (contextPtr ? 1 : 0)], expectedTy);
            }
        }
    }

    return static_cast<llvm::Value*>(result);
}

llvm::Value* IRGen::coerceArrayArg(llvm::Value* argVal, llvm::Type* expectedTy) {
    if (!argVal || !expectedTy) return argVal;
    if (!argVal->getType()->isArrayTy()) return argVal;

    // []T param: lower an array to a { ptr, len } slice (shares the array's
    // stack storage; the callee must not outlive the call).
    if (expectedTy->isStructTy()) {
        auto* dstST = llvm::cast<llvm::StructType>(expectedTy);
        if (dstST->getNumElements() == 2 &&
            dstST->getElementType(0)->isPointerTy() &&
            dstST->getElementType(1)->isIntegerTy()) {
            auto* srcArrTy = llvm::cast<llvm::ArrayType>(argVal->getType());

            llvm::Value* srcAlloca = nullptr;
            llvm::LoadInst* deadLoad = nullptr;
            if (auto* li = llvm::dyn_cast<llvm::LoadInst>(argVal)) {
                srcAlloca = li->getPointerOperand();
                deadLoad = li;
            }
            if (!srcAlloca) {
                srcAlloca = builder_->CreateAlloca(argVal->getType(), nullptr, "arr.slice");
                builder_->CreateStore(argVal, srcAlloca);
            }

            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
            auto* zero = llvm::ConstantInt::get(i64Ty, 0);
            auto* elemPtr = builder_->CreateGEP(argVal->getType(), srcAlloca,
                                                 {zero, zero}, "arr.slice.ptr");

            auto* ptrFieldTy = dstST->getElementType(0);
            llvm::Value* ptrField = elemPtr;
            if (elemPtr->getType() != ptrFieldTy)
                ptrField = builder_->CreateBitCast(elemPtr, ptrFieldTy, "arr.slice.cast");

            auto* lenFieldTy = dstST->getElementType(1);
            auto* lenField = llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(lenFieldTy),
                                                    srcArrTy->getNumElements());

            llvm::Value* slice = llvm::UndefValue::get(dstST);
            slice = builder_->CreateInsertValue(slice, ptrField, {0}, "arr.slice.ins.ptr");
            slice = builder_->CreateInsertValue(slice, lenField, {1}, "arr.slice.ins.len");

            if (deadLoad && deadLoad->use_empty())
                deadLoad->eraseFromParent();
            return slice;
        }
    }

    // *T param: decay an array to a pointer to its first element.
    if (expectedTy->isPointerTy()) {
        llvm::Value* srcAlloca = nullptr;
        llvm::LoadInst* deadLoad = nullptr;
        if (auto* li = llvm::dyn_cast<llvm::LoadInst>(argVal)) {
            srcAlloca = li->getPointerOperand();
            deadLoad = li;
        }
        if (!srcAlloca) {
            srcAlloca = builder_->CreateAlloca(argVal->getType(), nullptr, "arr.decay");
            builder_->CreateStore(argVal, srcAlloca);
        }
        auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
        auto* ptr = builder_->CreateGEP(argVal->getType(), srcAlloca, {zero, zero}, "arr.ptr");
        if (deadLoad && deadLoad->use_empty())
            deadLoad->eraseFromParent();
        return ptr;
    }

    return argVal;
}


