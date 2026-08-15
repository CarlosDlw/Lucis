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



std::any IRGen::visitConstDeclStmt(LucisParser::ConstDeclStmtContext* ctx) {
    if (!irDeclActive(ctx->attributeList(), targetTriple_, debugOutput_)) return {};
        SET_DBG_LOC(ctx);
    auto decls = ctx->constDeclarator();
    if (decls.empty()) return nullptr;

    bool isTopLevel = (currentFunction_ == nullptr);

    for (auto* d : decls) {
        auto name = d->IDENTIFIER()->getText();

        if (!d->expression()) continue;

        // Determine type: explicit or inferred from expression
        const TypeInfo* typeInfo = nullptr;
        unsigned dims = 0;

        if (d->typeSpec()) {
            typeInfo = resolveTypeInfo(d->typeSpec());
        } else {
            // Infer type from expression
            typeInfo = resolveExprTypeInfo(d->expression());
        }

        if (!typeInfo) continue;

        auto* llvmType = typeInfo->toLLVMType(*context_, module_->getDataLayout());

        if (isTopLevel) {
            // Top-level const: create global variable (initialized via init function)
            // ExternalLinkage allows the global to be referenced by other modules
            // that import this constant via `use`.
            auto* global = new llvm::GlobalVariable(
                *module_,
                llvmType,
                false,  // isConstant = false (allow init function to write)
                llvm::GlobalValue::ExternalLinkage,
                llvm::Constant::getNullValue(llvmType),
                "const_" + name
            );

            // Apply attributes on const decl
            auto* attrs = ctx->attributeList();
            // #[link_section("...")]
            auto linkSect = getAttributeStringArg(attrs, "link_section");
            if (!linkSect.empty())
                global->setSection(linkSect);
            // #[thread_local]
            if (hasAttribute(attrs, "thread_local"))
                global->setThreadLocal(true);
            // #[align(n)]
            if (hasAttribute(attrs, "align")) {
                for (auto* a : attrs->attribute()) {
                    if (a->IDENTIFIER() && a->IDENTIFIER()->getText() == "align") {
                        if (auto* argList = a->attrArgList()) {
                            for (auto* arg : argList->attrArg()) {
                                if (arg->INT_LIT()) {
                                    uint64_t alignVal = std::stoull(arg->getText());
                                    global->setAlignment(llvm::Align(alignVal));
                                }
                            }
                        }
                    }
                }
            }
            // #[used]
            if (hasAttribute(attrs, "used")) {
                global->setVisibility(llvm::GlobalValue::DefaultVisibility);
                llvm::appendToUsed(*module_, {global});
            }

            TopLevelConst tlc{global, typeInfo, dims};
            topLevelConsts_[name] = tlc;
            pendingConstDecls_.push_back({d, global});

            // Emit debug info for this global constant
            if (dbgBuilder_) {
                auto* diType = getOrCreateDIType(typeInfo);
                if (diType) {
                    unsigned constLine = d->IDENTIFIER()->getSymbol()
                        ? d->IDENTIFIER()->getSymbol()->getLine() : 0;
                    auto* gve = dbgBuilder_->createGlobalVariableExpression(
                        dbgCU_, name, "const_" + name,
                        dbgFile_, constLine,
                        diType, false, true);
                    global->addDebugInfo(gve);
                }
            }

            // Store compile-time integer literal value for use in array dimensions
            if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(d->expression())) {
                try {
                    compileTimeValues_[name] = std::stoll(intLit->getText());
                } catch (const std::out_of_range&) {
                    // Value too large for int64_t - skip
                }
            }
        } else {
            // Function-scoped const: alloca + store (like a local variable)
            auto* initVal = castValue(visit(d->expression()));
            if (!initVal) continue;

            // Coerce the value to the destination type if needed
            bool srcSigned = true;
            if (auto* srcTI = resolveExprTypeInfo(d->expression())) {
                if (srcTI->kind == TypeKind::Integer) srcSigned = srcTI->isSigned;
            }
            initVal = coerceValueToType(initVal, llvmType, srcSigned);

            auto* alloca = builder_->CreateAlloca(llvmType, nullptr, name);
            builder_->CreateStore(initVal, alloca);
            locals_[name] = {alloca, typeInfo, dims, false};

            // Store compile-time integer literal value for use in array dimensions
            if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(d->expression())) {
                try {
                    compileTimeValues_[name] = std::stoll(intLit->getText());
                } catch (const std::out_of_range&) {
                    // Value too large for int64_t - skip
                }
            }
        }
    }

    return nullptr;
}

// int32 x = 42;   or   []int32 arr = [1, 2, 3];   or   Vec<int32> v = [1, 2, 3];


std::any IRGen::visitVarDeclStmt(LucisParser::VarDeclStmtContext* ctx) {
    if (!irDeclActive(ctx->attributeList(), targetTriple_, debugOutput_)) return {};
        SET_DBG_LOC(ctx);
    // ── Tuple destructuring: auto (x, y) = expr; ────────────────────
    if (ctx->LPAREN()) {
        auto ids = ctx->IDENTIFIER();
        auto  initVal = visit(ctx->expression());
        auto* val     = castValue(initVal);
        auto* tupleTI = resolveExprTypeInfo(ctx->expression());

        for (size_t i = 0; i < ids.size(); i++) {
            auto varName = ids[i]->getText();
            auto* elemVal = builder_->CreateExtractValue(val, {(unsigned)i},
                                                          varName);
            auto* alloca = builder_->CreateAlloca(elemVal->getType(), nullptr,
                                                   varName);
            builder_->CreateStore(elemVal, alloca);
            const TypeInfo* elemTI = (tupleTI && tupleTI->kind == TypeKind::Tuple
                                       && i < tupleTI->tupleElements.size())
                                      ? tupleTI->tupleElements[i]
                                      : typeRegistry_.lookup("int32");
            locals_[varName] = { alloca, elemTI, 0 };
            emitDbgDeclare(alloca, varName, elemTI, ids[i]->getSymbol() ? ids[i]->getSymbol()->getLine() : 0);
        }
        return {};
    }

    bool hasNsPrefix = ctx->SCOPE() != nullptr;

    // ── Detect auto type ─────────────────────────────────────────────
    bool isAutoType = (ctx->typeSpec() && ctx->typeSpec()->AUTO());

    // ── Resolve explicit type once ───────────────────────────────────
    const TypeInfo* ti = nullptr;
    unsigned dims = 0;
    std::vector<unsigned> aliasArraySizes;
    if (!isAutoType) {
        ti = [&]() -> const TypeInfo* {
            if (!currentGenericSubst_.empty())
                return resolveTypeInfoWithSubst(ctx->typeSpec(), currentGenericSubst_);
            return resolveTypeInfo(ctx->typeSpec());
        }();
        dims = countArrayDims(ctx->typeSpec());

        if (dims == 0 && ti && !ti->arraySizes.empty() && ti->elementType) {
            dims = static_cast<unsigned>(ti->arraySizes.size());
            aliasArraySizes = ti->arraySizes;
            ti = ti->elementType;
        }
    }

    // ── Iterate varDeclarators ──────────────────────────────────────
    auto decls = ctx->varDeclarator();

    // Pre-compute the last initializer value for propagation to vars without init
    llvm::Value* lastInitVal = nullptr;
    LucisParser::VarDeclaratorContext* lastInitDecl = nullptr;
    {
        for (auto it = decls.rbegin(); it != decls.rend(); ++it) {
            if ((*it)->expression()) { lastInitDecl = *it; break; }
        }
        if (lastInitDecl) {
            if (isAutoType) {
                auto raw = visit(lastInitDecl->expression());
                auto* val = castValue(raw);
                auto* ti2 = resolveExprTypeInfo(lastInitDecl->expression());
                if (!ti2) ti2 = typeRegistry_.lookup("int32");
                auto* type = ti2 ? ti2->toLLVMType(*context_, module_->getDataLayout()) : val->getType();
                auto* srcTI = resolveExprTypeInfo(lastInitDecl->expression());
                bool srcSigned = !val->getType()->isIntegerTy(1);
                if (srcTI && srcTI->kind == TypeKind::Integer) srcSigned = srcTI->isSigned;
                lastInitVal = coerceValueToType(val, type, srcSigned);
            } else if (ti && ti->kind != TypeKind::Extended) {
                auto raw = visit(lastInitDecl->expression());
                auto* val = castValue(raw);
                auto* type = ti->toLLVMType(*context_, module_->getDataLayout());
                auto* srcTI = resolveExprTypeInfo(lastInitDecl->expression());
                bool srcSigned = !val->getType()->isIntegerTy(1);
                if (srcTI && srcTI->kind == TypeKind::Integer) srcSigned = srcTI->isSigned;
                lastInitVal = coerceValueToType(val, type, srcSigned);
            }
        }
    }

    for (auto* d : decls) {
        auto name = d->IDENTIFIER()->getText();

        if (isAutoType) {
            if (!d->expression()) {
                // Propagate last init value
                auto* ti2 = typeRegistry_.lookup("int32");
                auto* type = ti2->toLLVMType(*context_, module_->getDataLayout());
                auto* alloca = builder_->CreateAlloca(type, nullptr, name);
                if (lastInitVal)
                    builder_->CreateStore(lastInitVal, alloca);
                else
                    builder_->CreateStore(llvm::Constant::getNullValue(type), alloca);
                VarInfo vi{ alloca, ti2, 0 };
                locals_[name] = std::move(vi);
                emitDbgDeclare(alloca, name, vi.typeInfo, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);
                if (auto* idTok = d->IDENTIFIER() ? d->IDENTIFIER()->getSymbol() : nullptr)
                    emitDbgDeclare(alloca, name, ti2, idTok->getLine());
                continue;
            }
            llvm::Value* val = nullptr;
            unsigned exprDims = 0;
            const TypeInfo* ti2 = nullptr;

            if (d == lastInitDecl && lastInitVal) {
                // Use pre-computed value from lastInitDecl to avoid double-visiting
                // the expression (which would consume resources like vec args twice).
                val = lastInitVal;
                ti2 = resolveExprTypeInfo(d->expression());
                exprDims = resolveExprArrayDims(d->expression());
            } else {
                auto raw = visit(d->expression());
                val = castValue(raw);
                exprDims = resolveExprArrayDims(d->expression());
                ti2 = resolveExprTypeInfo(d->expression());
            }
            if (!ti2)
                ti2 = typeRegistry_.lookup("int32");

            if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(d->expression())) {
                auto elems = arrLit->expression();
                if (!elems.empty()) {
                    auto* elemTI = resolveExprTypeInfo(elems[0]);
                    if (!elemTI) elemTI = typeRegistry_.lookup("int32");
                    auto* elemType = elemTI->toLLVMType(*context_, module_->getDataLayout());
                    auto* targetTy = buildTargetArrayType(val->getType(), elemType);
                    auto* alloca   = builder_->CreateAlloca(targetTy, nullptr, name);
                    storeArrayElements(val, alloca, targetTy, elemTI, 1);
                    locals_[name] = { alloca, elemTI, 1 };
                    emitDbgDeclare(alloca, name, elemTI, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);
                    continue;
                }
            }

            if (exprDims > 0 && val && val->getType()->isArrayTy()) {
                auto* elemTI = ti2 ? ti2 : typeRegistry_.lookup("int32");
                auto* elemType = elemTI->toLLVMType(*context_, module_->getDataLayout());
                auto* targetTy = buildTargetArrayType(val->getType(), elemType);
                auto* alloca   = builder_->CreateAlloca(targetTy, nullptr, name);
                storeArrayElements(val, alloca, targetTy, elemTI, exprDims);
                locals_[name] = { alloca, elemTI, exprDims };
                emitDbgDeclare(alloca, name, elemTI, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);
                continue;
            }

            auto* type = ti2 ? ti2->toLLVMType(*context_, module_->getDataLayout())
                             : val->getType();
            auto* srcTI = resolveExprTypeInfo(d->expression());
            bool srcSigned = true;
            if (srcTI && srcTI->kind == TypeKind::Integer)
                srcSigned = srcTI->isSigned;
            else if (val->getType()->isIntegerTy(1))
                srcSigned = false;
            val = coerceValueToType(val, type, srcSigned);
            auto* alloca = builder_->CreateAlloca(type, nullptr, name);
            builder_->CreateStore(val, alloca);
            VarInfo vi{ alloca, ti2, exprDims };
            // Structs own their string fields only when built from a
            // literal; any other source (at(), function returns, copies)
            // is a shallow copy — freeing the fields double-frees the
            // original owner's buffers.
            vi.isBorrowed = (ti2 && ti2->kind == TypeKind::String &&
                     isBorrowedStringValueExpr(d->expression())) ||
                    (ti2 && ti2->kind == TypeKind::Struct &&
                     !isStructLiteralExpr(d->expression()));
            locals_[name] = std::move(vi);
            emitDbgDeclare(alloca, name, vi.typeInfo, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);
            continue;
        }

        // ── Explicit type ────────────────────────────────────────────
        if (!ti) continue;

        if (ti->kind == TypeKind::Extended) {
            auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);

            if (ti->extendedKind == "Task") {
                auto* alloca = builder_->CreateAlloca(ptrTy, nullptr, name);
                locals_[name] = { alloca, ti, 0 };
                emitDbgDeclare(alloca, name, ti, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);
                if (d->expression()) {
                    auto  initVal2 = visit(d->expression());
                    auto* val2     = castValue(initVal2);
                    builder_->CreateStore(val2, alloca);
                } else {
                    builder_->CreateStore(
                        llvm::ConstantPointerNull::get(ptrTy), alloca);
                }
                continue;
            }

            if (ti->extendedKind == "Mutex") {
                auto* alloca = builder_->CreateAlloca(ptrTy, nullptr, name);
                locals_[name] = { alloca, ti, 0 };
                emitDbgDeclare(alloca, name, ti, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);
                auto callee = declareBuiltin("lucis_mutexCreate", ptrTy, {});
                auto* mtx = builder_->CreateCall(callee, {}, "mutex");
                builder_->CreateStore(mtx, alloca);
                continue;
            }

            if (ti->extendedKind == "Map") {
                auto* mapTy  = getOrCreateMapStructType();
                auto  suffix = ti->builtinSuffix;
                auto* alloca = builder_->CreateAlloca(mapTy, nullptr, name);
                locals_[name] = { alloca, ti, 0 };
                emitDbgDeclare(alloca, name, ti, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);

                if (suffix.size() > 4 && suffix.substr(suffix.size() - 3) == "raw") {
                    auto* valLLTy = ti->valueType->toLLVMType(*context_, module_->getDataLayout());
                    auto valSz = module_->getDataLayout().getTypeAllocSize(valLLTy);
                    auto initFn = declareBuiltin("lucis_map_init_" + suffix,
                        llvm::Type::getVoidTy(*context_), { ptrTy, usizeTy });
                    builder_->CreateCall(initFn, { alloca, llvm::ConstantInt::get(usizeTy, valSz) });
                } else {
                    auto initFn = declareBuiltin("lucis_map_init_" + suffix,
                        llvm::Type::getVoidTy(*context_), { ptrTy });
                    builder_->CreateCall(initFn, { alloca });
                }
                // Fallback: bang-init (skip empty) or other expression initializer
                if (d->expression()) {
                    auto* bangInit = dynamic_cast<LucisParser::BangInitExprContext*>(d->expression());
                    if (!bangInit || !bangInit->bangEntry().empty()) {
                        auto* initVal = castValue(visit(d->expression()));
                        builder_->CreateStore(initVal, alloca);
                    }
                }
                continue;
            }

            if (ti->extendedKind == "Set") {
                auto* setTy  = getOrCreateSetStructType();
                if (!ti->elementType) {
                    std::cerr << "lucis: internal error: Set missing elementType\n";
                    continue;
                }
                auto  suffix = getVecSuffix(ti->elementType);
                auto* alloca = builder_->CreateAlloca(setTy, nullptr, name);
                locals_[name] = { alloca, ti, 0 };
                emitDbgDeclare(alloca, name, ti, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);

                auto* elemLLTy = ti->elementType->toLLVMType(*context_, module_->getDataLayout());
                if (suffix == "raw") {
                    auto elemSz = module_->getDataLayout().getTypeAllocSize(elemLLTy);
                    auto initFn = declareBuiltin("lucis_set_init_raw",
                        llvm::Type::getVoidTy(*context_), { ptrTy, usizeTy });
                    builder_->CreateCall(initFn, { alloca, llvm::ConstantInt::get(usizeTy, elemSz) });
                } else {
                    auto initFn = declareBuiltin("lucis_set_init_" + suffix,
                        llvm::Type::getVoidTy(*context_), { ptrTy });
                    builder_->CreateCall(initFn, { alloca });
                }

                if (d->expression()) {
                    auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(d->expression());
                    if (arrLit && !arrLit->expression().empty()) {
                        if (suffix == "raw") {
                            auto addFn = declareBuiltin("lucis_set_add_raw",
                                llvm::Type::getInt32Ty(*context_), { ptrTy, ptrTy });
                            for (auto* e : arrLit->expression()) {
                                auto* val2 = castValue(visit(e));
                                auto* tmp = builder_->CreateAlloca(elemLLTy);
                                builder_->CreateStore(val2, tmp);
                                builder_->CreateCall(addFn, { alloca, tmp });
                                consumeExprIfOwnedLocal(e);
                            }
                        } else {
                            auto addFn = declareBuiltin("lucis_set_add_" + suffix,
                                llvm::Type::getInt32Ty(*context_), { ptrTy, elemLLTy });
                            for (auto* e : arrLit->expression()) {
                                auto* val2 = castValue(visit(e));
                                if (val2->getType() != elemLLTy) {
                                    if (val2->getType()->isIntegerTy() && elemLLTy->isIntegerTy())
                                        val2 = builder_->CreateIntCast(val2, elemLLTy, ti->elementType->isSigned);
                                }
                                builder_->CreateCall(addFn, { alloca, val2 });
                                consumeExprIfOwnedLocal(e);
                            }
                        }
                    } else {
                        // Fallback: bang-init (skip empty) or other expression initializer
                        auto* bangInit = dynamic_cast<LucisParser::BangInitExprContext*>(d->expression());
                        if (!bangInit || !bangInit->bangEntry().empty()) {
                            auto* initVal = castValue(visit(d->expression()));
                            builder_->CreateStore(initVal, alloca);
                        }
                    }
                }
                continue;
            }

            // Vec<T>
            auto* vecTy  = getOrCreateVecStructType();
            if (!ti->elementType) {
                std::cerr << "lucis: internal error: Vec missing elementType\n";
                continue;
            }
            auto suffix = getVecSuffix(ti->elementType);

            auto* alloca = builder_->CreateAlloca(vecTy, nullptr, name);
            locals_[name] = { alloca, ti, 0 };
            emitDbgDeclare(alloca, name, ti, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);

            // Always init the vec first (empty init by default)
            auto initFn = declareBuiltin("lucis_vec_init_" + suffix,
                llvm::Type::getVoidTy(*context_), { ptrTy });
            builder_->CreateCall(initFn, { alloca });

            if (!d->expression()) {
                continue;
            }

            auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(d->expression());

            if (arrLit) {
                std::vector<llvm::Value*> vals;
                for (auto* e : arrLit->expression()) {
                    vals.push_back(castValue(visit(e)));
                    consumeExprIfOwnedLocal(e);
                }

                auto* elemLLTy = ti->elementType->toLLVMType(*context_, module_->getDataLayout());

                if (!vals.empty()) {
                    if (suffix == "raw") {
                        auto& dl       = module_->getDataLayout();
                        auto  elemSz   = dl.getTypeAllocSize(elemLLTy);
                        auto* elemSzVal = llvm::ConstantInt::get(usizeTy, elemSz);

                        auto initCapFn = declareBuiltin("lucis_vec_init_cap_raw",
                            llvm::Type::getVoidTy(*context_), { ptrTy, usizeTy, usizeTy });
                        builder_->CreateCall(initCapFn, { alloca,
                            llvm::ConstantInt::get(usizeTy, vals.size()), elemSzVal });

                        auto pushFn = declareBuiltin("lucis_vec_push_raw",
                            llvm::Type::getVoidTy(*context_), { ptrTy, ptrTy, usizeTy });
                        for (auto* v : vals) {
                            auto* tmp = builder_->CreateAlloca(elemLLTy);
                            builder_->CreateStore(v, tmp);
                            builder_->CreateCall(pushFn, { alloca, tmp, elemSzVal });
                        }
                    } else {
                        auto initCapFn = declareBuiltin("lucis_vec_init_cap_" + suffix,
                            llvm::Type::getVoidTy(*context_), { ptrTy, usizeTy });
                        builder_->CreateCall(initCapFn, { alloca,
                            llvm::ConstantInt::get(usizeTy, vals.size()) });

                        auto pushFn = declareBuiltin("lucis_vec_push_" + suffix,
                            llvm::Type::getVoidTy(*context_), { ptrTy, elemLLTy });
                        for (auto* v : vals) {
                            if (v->getType() != elemLLTy) {
                                if (v->getType()->isIntegerTy() && elemLLTy->isIntegerTy())
                                    v = builder_->CreateIntCast(v, elemLLTy, ti->elementType->isSigned);
                            }
                            builder_->CreateCall(pushFn, { alloca, v });
                        }
                    }
                }
            } else if (auto* bangInit = dynamic_cast<LucisParser::BangInitExprContext*>(d->expression())) {
                if (!bangInit->bangEntry().empty()) {
                    auto initVal2 = visit(d->expression());
                    auto* val2 = castValue(initVal2);
                    builder_->CreateStore(val2, alloca);
                }
                // Empty bang-init: skip — alloca already initialized above
            } else {
                auto initVal2 = visit(d->expression());
                auto* val2 = castValue(initVal2);
                builder_->CreateStore(val2, alloca);
            }
            continue;
        }

        if (dims > 0 && (!ti || ti->kind != TypeKind::Pointer)) {
            auto* elemType = ti->toLLVMType(*context_, module_->getDataLayout());

            if (!d->expression()) {
                auto* spec = ctx->typeSpec();
                std::vector<unsigned> sizes;
                while (!spec->typeSpec().empty() && (spec->INT_LIT() || spec->IDENTIFIER())) {
                    if (spec->INT_LIT()) {
                        sizes.push_back(std::stoul(spec->INT_LIT()->getText()));
                    } else {
                        auto identName = spec->IDENTIFIER()->getText();
                        auto cit = compileTimeValues_.find(identName);
                        if (cit != compileTimeValues_.end()) {
                            sizes.push_back(static_cast<unsigned>(cit->second));
                        } else {
                            break;
                        }
                    }
                    spec = spec->typeSpec(0);
                }
                if (sizes.empty() && !aliasArraySizes.empty())
                    sizes = aliasArraySizes;
                if (sizes.empty()) {
                    std::cerr << "lucis: array variable '" << name
                              << "' requires explicit dimensions\n";
                    continue;
                }
                for (auto sz : sizes) {
                    if (sz == 0) {
                        std::cerr << "lucis: array dimension cannot be zero\n";
                        continue;
                    }
                }
                llvm::Type* arrTy = elemType;
                for (auto it = sizes.rbegin(); it != sizes.rend(); ++it)
                    arrTy = llvm::ArrayType::get(arrTy, *it);
                auto* alloca = builder_->CreateAlloca(arrTy, nullptr, name);
                {
                    uint64_t totalBytes = module_->getDataLayout().getTypeAllocSize(arrTy);
                    if (totalBytes > 64) {
                        auto* sizeVal = llvm::ConstantInt::get(
                            llvm::Type::getInt64Ty(*context_), totalBytes);
                        builder_->CreateMemSet(alloca, builder_->getInt8(0),
                                              sizeVal, llvm::MaybeAlign(1));
                    } else {
                        builder_->CreateStore(llvm::Constant::getNullValue(arrTy), alloca);
                    }
                }
                locals_[name] = { alloca, ti, dims };
                emitDbgDeclare(alloca, name, ti, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);
                continue;
            }

            auto  initVal = visit(d->expression());
            auto* val     = castValue(initVal);

            if (!val->getType()->isArrayTy()) {
                auto* spec = ctx->typeSpec();
                std::vector<unsigned> sizes;
                while (!spec->typeSpec().empty() && (spec->INT_LIT() || spec->IDENTIFIER())) {
                    if (spec->INT_LIT()) {
                        sizes.push_back(std::stoul(spec->INT_LIT()->getText()));
                    } else {
                        auto identName = spec->IDENTIFIER()->getText();
                        auto cit = compileTimeValues_.find(identName);
                        if (cit != compileTimeValues_.end()) {
                            sizes.push_back(static_cast<unsigned>(cit->second));
                        } else {
                            break;
                        }
                    }
                    spec = spec->typeSpec(0);
                }
                if (sizes.empty() && !aliasArraySizes.empty())
                    sizes = aliasArraySizes;
                // Dynamic array []T with empty initializer → {null, 0}
                if (sizes.empty() && dims > 0) {
                    // Check if initializer is a c-string: []uint8 = c"..." → infer size
                    if (dynamic_cast<LucisParser::CStrLitExprContext*>(d->expression())) {
                        if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(val->stripPointerCasts())) {
                            if (auto* cda = llvm::dyn_cast_or_null<llvm::ConstantDataArray>(gv->getInitializer())) {
                                sizes.push_back(cda->getNumElements());
                            }
                        }
                    }
                }
                if (sizes.empty() && dims > 0) {
                    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                    auto* i64Ty = llvm::Type::getInt64Ty(*context_);
                    auto* sliceTy = llvm::StructType::get(*context_, {ptrTy, i64Ty});
                    auto* nullPtr = llvm::ConstantPointerNull::get(ptrTy);
                    auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                    llvm::Value* slice = llvm::UndefValue::get(sliceTy);
                    slice = builder_->CreateInsertValue(slice, nullPtr, {0});
                    slice = builder_->CreateInsertValue(slice, zero, {1});
                    auto* alloca = builder_->CreateAlloca(sliceTy, nullptr, name);
                    builder_->CreateStore(slice, alloca);
                    locals_[name] = { alloca, ti, dims };
                    emitDbgDeclare(alloca, name, ti, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);
                    continue;
                }
                llvm::Type* arrTy = elemType;
                for (auto it = sizes.rbegin(); it != sizes.rend(); ++it)
                    arrTy = llvm::ArrayType::get(arrTy, *it);
                auto* alloca = builder_->CreateAlloca(arrTy, nullptr, name);
                // [N]uint8 = c"..." → memcpy from global string
                if (dynamic_cast<LucisParser::CStrLitExprContext*>(d->expression())) {
                    auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
                    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                    auto* voidTy  = llvm::Type::getVoidTy(*context_);
                    auto* dst = builder_->CreateConstGEP2_32(arrTy, alloca, 0, 0, name + ".gep");
                    auto* dstB = builder_->CreateBitCast(dst, ptrTy, name + ".dst");
                    auto* srcB = builder_->CreateBitCast(val, ptrTy, name + ".src");
                    auto memcpyFn = declareBuiltin("memcpy", ptrTy, {ptrTy, ptrTy, usizeTy});
                    builder_->CreateCall(memcpyFn, {dstB, srcB,
                        llvm::ConstantInt::get(usizeTy, module_->getDataLayout().getTypeAllocSize(arrTy))},
                        name + ".cpy");
                } else {
                    builder_->CreateStore(llvm::Constant::getNullValue(arrTy), alloca);
                }
                locals_[name] = { alloca, ti, dims };
                emitDbgDeclare(alloca, name, ti, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);
                continue;
            }

            auto* targetTy = buildTargetArrayType(val->getType(), elemType);
            auto* alloca   = builder_->CreateAlloca(targetTy, nullptr, name);
            storeArrayElements(val, alloca, targetTy, ti, dims);
            locals_[name] = { alloca, ti, dims };
            emitDbgDeclare(alloca, name, ti, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);
        } else {
            // Scalar variable
            auto* type   = ti->toLLVMType(*context_, module_->getDataLayout());
            auto* alloca = builder_->CreateAlloca(type, nullptr, name);
            unsigned trackedDims = (ti && ti->kind == TypeKind::Pointer) ? dims : 0;
            std::vector<unsigned> pointerArraySizes;
            if (ti && ti->kind == TypeKind::Pointer && ctx->typeSpec() &&
                ctx->typeSpec()->STAR() && !ctx->typeSpec()->typeSpec().empty()) {
                auto* inner = ctx->typeSpec()->typeSpec(0);
                while (inner && inner->LBRACKET()) {
                    if (!inner->INT_LIT()) break;
                    pointerArraySizes.push_back(std::stoul(inner->INT_LIT()->getText()));
                    if (inner->typeSpec().empty()) break;
                    inner = inner->typeSpec(0);
                }
            }
            VarInfo vi{ alloca, ti, trackedDims };
            vi.isBorrowed = (ti && ti->kind == TypeKind::String &&
                     isBorrowedStringValueExpr(d->expression())) ||
                    (ti && ti->kind == TypeKind::Struct &&
                     !isStructLiteralExpr(d->expression()));
            vi.fixedArraySizes = std::move(pointerArraySizes);
            locals_[name] = std::move(vi);
            emitDbgDeclare(alloca, name, vi.typeInfo, d->IDENTIFIER()->getSymbol() ? d->IDENTIFIER()->getSymbol()->getLine() : 0);

            if (!d->expression()) {
                if (lastInitVal) {
                    builder_->CreateStore(lastInitVal, alloca);
                } else {
                    builder_->CreateStore(llvm::Constant::getNullValue(type), alloca);
                }
                continue;
            }

            if (d == lastInitDecl && lastInitVal) {
                builder_->CreateStore(lastInitVal, alloca);
                continue;
            }

            auto  initVal = visit(d->expression());
            auto* val     = castValue(initVal);
            auto* srcTI   = resolveExprTypeInfo(d->expression());

            bool srcSigned = !val->getType()->isIntegerTy(1);
            if (srcTI && srcTI->kind == TypeKind::Integer)
                srcSigned = srcTI->isSigned;
            val = coerceValueToType(val, type, srcSigned);

            builder_->CreateStore(val, alloca);

            // Store compile-time integer literal value for use in array dimensions
            if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(d->expression())) {
                try {
                    compileTimeValues_[name] = std::stoll(intLit->getText());
                } catch (const std::out_of_range&) {
                    // Value too large for int64_t - skip
                }
            }
        }
    }

    return {};
}

// x = 42;  or  arr[i] = val;   arr[i][j] = val;


std::any IRGen::visitAssignStmt(LucisParser::AssignStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto varName = ctx->IDENTIFIER()->getText();
    auto it = locals_.find(varName);

    // ── C global variable assignment ────────────────────────────────
    if (it == locals_.end()) {
        auto git = cGlobals_.find(varName);
        if (git != cGlobals_.end()) {
            auto& entry = git->second;
            auto exprs = ctx->expression();
            if (exprs.size() == 1) {
                auto* val = castValue(visit(exprs[0]));
                auto* gvTy = entry.global->getValueType();
                if (val->getType() != gvTy) {
                    if (val->getType()->isIntegerTy() && gvTy->isIntegerTy())
                        val = builder_->CreateIntCast(val, gvTy,
                                  entry.type->isSigned);
                }
                builder_->CreateStore(val, entry.global);
            }
            return {};
        }
        std::cerr << "lucis: undefined variable '" << varName << "'\n";
        return {};
    }

    auto* alloca    = it->second.alloca;
    auto* allocType = alloca->getAllocatedType();
    auto* elemTI    = it->second.typeInfo;

    auto exprs = ctx->expression();

    // Simple reassignment: x = 42; (no bracket expressions)
    if (exprs.size() == 1) {
        auto* val = castValue(visit(exprs[0]));
        auto* srcTI = resolveExprTypeInfo(exprs[0]);
        auto* varTy = allocType;

        if (val->getType() != varTy) {
            if (val->getType()->isIntegerTy() && varTy->isIntegerTy()) {
                bool srcSigned = !val->getType()->isIntegerTy(1);
                if (srcTI && srcTI->kind == TypeKind::Integer)
                    srcSigned = srcTI->isSigned;
                val = builder_->CreateIntCast(val, varTy, srcSigned);
            }
            else if (val->getType()->isFloatingPointTy() && varTy->isFloatingPointTy()) {
                if (val->getType()->getPrimitiveSizeInBits() > varTy->getPrimitiveSizeInBits())
                    val = builder_->CreateFPTrunc(val, varTy);
                else
                    val = builder_->CreateFPExt(val, varTy);
            }
        }

        builder_->CreateStore(val, alloca);
        if (elemTI && elemTI->kind == TypeKind::String)
            it->second.isBorrowed = isBorrowedStringValueExpr(exprs[0]);
        return {};
    }

    // Indexed assignment: arr[i] = val  or  vec[i] = val
    // All expressions except the last are indices; the last is the RHS value

    // ── User-defined opIndex assignment on struct ──────────────────────
    if (elemTI && elemTI->kind == TypeKind::Struct && exprs.size() >= 2) {
        IRGen::MethodEntry* setter = nullptr;
        for (auto* t = elemTI; t; t = t->parentType) {
            auto smIt = operatorMethods_.find(t->name);
            if (smIt == operatorMethods_.end()) continue;
            auto mIt = smIt->second.find("opIndex");
            if (mIt != smIt->second.end()) {
                for (auto& entry : mIt->second) {
                    if (entry.fn->arg_size() == 3)
                        setter = &entry;
                }
            }
        }
        if (setter) {
            auto* idxVal = castValue(visit(exprs[0]));
            auto* val = castValue(visit(exprs.back()));
            llvm::Value* selfPtr = alloca->getAllocatedType()->isPointerTy()
                ? static_cast<llvm::Value*>(
                    builder_->CreateLoad(llvm::PointerType::getUnqual(*context_), alloca))
                : static_cast<llvm::Value*>(alloca);
            return static_cast<llvm::Value*>(
                builder_->CreateCall(setter->fn, {selfPtr, idxVal, val}));
        }
    }

    // ── Extended type (Vec<T>, Map<K,V>) index assignment ──────────────
    llvm::Value* containerPtr = alloca;
    if (elemTI && elemTI->kind == TypeKind::Pointer &&
        elemTI->pointeeType && elemTI->pointeeType->kind == TypeKind::Extended) {
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        containerPtr = builder_->CreateLoad(ptrTy, alloca, varName + "_deref");
        elemTI = elemTI->pointeeType;
    }
    if (elemTI && elemTI->kind == TypeKind::Extended) {
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);

        if (elemTI->extendedKind == "Map") {
            // Map<K,V> subscript assignment: m[key] = val → lucis_map_set_<suffix>
            auto* keyLLTy = elemTI->keyType->toLLVMType(*context_, module_->getDataLayout());
            auto* valLLTy = elemTI->valueType->toLLVMType(*context_, module_->getDataLayout());
            auto suffix = elemTI->builtinSuffix;

            auto* keyVal = castValue(visit(exprs[0]));
            if (keyVal->getType() != keyLLTy) {
                if (keyVal->getType()->isIntegerTy() && keyLLTy->isIntegerTy())
                    keyVal = builder_->CreateIntCast(keyVal, keyLLTy, elemTI->keyType->isSigned);
            }

            auto* val = castValue(visit(exprs.back()));

            // Handle raw value types: pass by pointer
            bool isRawVal = suffix.size() > 4 && suffix.substr(suffix.size() - 3) == "raw";

            if (isRawVal) {
                auto* valAlloca = builder_->CreateAlloca(valLLTy, nullptr, "map_set_val");
                builder_->CreateStore(val, valAlloca);
                auto callee = declareBuiltin(
                    "lucis_map_set_" + suffix,
                    llvm::Type::getVoidTy(*context_),
                    { ptrTy, keyLLTy, ptrTy });
                builder_->CreateCall(callee, { containerPtr, keyVal, valAlloca });
            } else {
                if (val->getType() != valLLTy) {
                    if (val->getType()->isIntegerTy() && valLLTy->isIntegerTy())
                        val = builder_->CreateIntCast(val, valLLTy, elemTI->valueType->isSigned);
                    else if (val->getType()->isFloatingPointTy() && valLLTy->isFloatingPointTy()) {
                        if (val->getType()->getPrimitiveSizeInBits() > valLLTy->getPrimitiveSizeInBits())
                            val = builder_->CreateFPTrunc(val, valLLTy);
                        else
                            val = builder_->CreateFPExt(val, valLLTy);
                    }
                }
                auto callee = declareBuiltin(
                    "lucis_map_set_" + suffix,
                    llvm::Type::getVoidTy(*context_),
                    { ptrTy, keyLLTy, valLLTy });
                builder_->CreateCall(callee, { containerPtr, keyVal, val });
            }
            return {};
        }

        // Vec<T> index assignment: v[i] = x → lucis_vec_set_<suffix>
        if (!elemTI->elementType) {
            std::cerr << "lucis: internal error: Vec missing elementType for index assignment\n";
            return {};
        }
        auto* elemLLTy = elemTI->elementType->toLLVMType(
            *context_, module_->getDataLayout());
        auto suffix = getVecSuffix(elemTI->elementType);

        auto* idx = castValue(visit(exprs[0]));
        if (idx->getType() != usizeTy)
            idx = builder_->CreateIntCast(idx, usizeTy, false);

        auto* val = castValue(visit(exprs.back()));
        if (val->getType() != elemLLTy) {
            if (val->getType()->isIntegerTy() && elemLLTy->isIntegerTy())
                val = builder_->CreateIntCast(val, elemLLTy,
                    elemTI->elementType->isSigned);
            else if (val->getType()->isFloatingPointTy() &&
                     elemLLTy->isFloatingPointTy()) {
                if (val->getType()->getPrimitiveSizeInBits() >
                    elemLLTy->getPrimitiveSizeInBits())
                    val = builder_->CreateFPTrunc(val, elemLLTy);
                else
                    val = builder_->CreateFPExt(val, elemLLTy);
            }
        }

        if (suffix == "raw") {
            auto& dl     = module_->getDataLayout();
            auto  elemSz = dl.getTypeAllocSize(elemLLTy);
            auto* elemSzVal = llvm::ConstantInt::get(usizeTy, elemSz);
            auto ptrFn = declareBuiltin("lucis_vec_ptr_raw", ptrTy,
                                        {ptrTy, usizeTy, usizeTy});
            auto* elemPtr = builder_->CreateCall(ptrFn,
                { containerPtr, idx, elemSzVal }, "vec_elem_ptr");
            builder_->CreateStore(val, elemPtr);
            return {};
        }

        auto callee = declareBuiltin(
            "lucis_vec_set_" + suffix,
            llvm::Type::getVoidTy(*context_),
            { ptrTy, usizeTy, elemLLTy });
        builder_->CreateCall(callee, { containerPtr, idx, val });
        return {};
    }

    // ── Pointer index assignment: ptr[i] = val ─────────────────────
    if (elemTI && elemTI->kind == TypeKind::Pointer && elemTI->pointeeType) {
        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);

        // Load pointer value from variable storage.
        auto* ptrVal = builder_->CreateLoad(ptrTy, alloca, varName + "_ptr");
        auto* val = castValue(visit(exprs.back()));

        llvm::Type* currentTy = elemTI->pointeeType->toLLVMType(*context_, module_->getDataLayout());
        llvm::Value* currentPtr = ptrVal;

        for (size_t i = 0; i + 1 < exprs.size(); i++) {
            auto* idx = castValue(visit(exprs[i]));
            if (idx->getType() != i64Ty)
                idx = builder_->CreateIntCast(idx, i64Ty, true);

            if (auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(currentTy)) {
                currentPtr = builder_->CreateInBoundsGEP(
                    arrTy,
                    currentPtr,
                    { llvm::ConstantInt::get(i64Ty, 0), idx },
                    varName + "_elem");
                currentTy = arrTy->getElementType();
            } else {
                currentPtr = builder_->CreateInBoundsGEP(
                    currentTy,
                    currentPtr,
                    idx,
                    varName + "_elem");
            }
        }

        if (val->getType() != currentTy) {
            if (val->getType()->isIntegerTy() && currentTy->isIntegerTy())
                val = builder_->CreateIntCast(val, currentTy, true);
            else if (val->getType()->isFloatingPointTy() && currentTy->isFloatingPointTy()) {
                if (val->getType()->getPrimitiveSizeInBits() > currentTy->getPrimitiveSizeInBits())
                    val = builder_->CreateFPTrunc(val, currentTy);
                else
                    val = builder_->CreateFPExt(val, currentTy);
            }
        }

        builder_->CreateStore(val, currentPtr);
        return {};
    }

    // ── Slice index assignment: arr[i] = val  ([]T param/local) ─────
    // Slice params and slice-from-slice locals are stored as {ptr, len}
    // structs (not LLVM arrays), so they need this path instead of the
    // ArrayType path below (which would crash on a bad cast). Mirrors
    // the slice read path in IRGen_access.cpp.
    if (it->second.arrayDims == 1 && allocType->isStructTy() &&
        exprs.size() == 2) {
        if (!elemTI) {
            std::cerr << "lucis: internal error: slice element type unknown for '"
                      << varName << "'\n";
            return {};
        }
        auto* sliceVal = builder_->CreateLoad(allocType, alloca, varName);
        auto* dataPtr  = builder_->CreateExtractValue(sliceVal, 0, "slice_ptr");
        auto* lenVal   = builder_->CreateExtractValue(sliceVal, 1, "slice_len");

        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
        auto* idx = castValue(visit(exprs[0]));
        if (idx->getType() != i64Ty)
            idx = builder_->CreateIntCast(idx, i64Ty, true);

        // Bounds check (same pattern as the slice read path)
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* idxU = builder_->CreateIntCast(idx, usizeTy, false, "bnd.idx");
        auto* lenU = builder_->CreateIntCast(lenVal, usizeTy, false, "bnd.len");
        emitBoundsCheck(idxU, lenU, "slice_idx_assign");

        auto* elemTy = elemTI->toLLVMType(*context_, module_->getDataLayout());
        auto* elemPtr = builder_->CreateInBoundsGEP(
            elemTy, dataPtr, idx, varName + "_elem");

        auto* val = castValue(visit(exprs.back()));
        if (val->getType() != elemTy) {
            if (val->getType()->isIntegerTy() && elemTy->isIntegerTy())
                val = builder_->CreateIntCast(val, elemTy, elemTI->isSigned);
            else if (val->getType()->isFloatingPointTy() && elemTy->isFloatingPointTy()) {
                if (val->getType()->getPrimitiveSizeInBits() > elemTy->getPrimitiveSizeInBits())
                    val = builder_->CreateFPTrunc(val, elemTy);
                else
                    val = builder_->CreateFPExt(val, elemTy);
            }
        }

        builder_->CreateStore(val, elemPtr);
        return {};
    }

    // ── Array index assignment (original path) ──────────────────────
    auto* val = castValue(visit(exprs.back()));

    auto* i64Ty = llvm::Type::getInt64Ty(*context_);
    std::vector<llvm::Value*> gepIndices;
    gepIndices.push_back(llvm::ConstantInt::get(i64Ty, 0));
    for (size_t i = 0; i + 1 < exprs.size(); i++) {
        auto* idx = castValue(visit(exprs[i]));
        if (idx->getType() != i64Ty)
            idx = builder_->CreateIntCast(idx, i64Ty, true);
        gepIndices.push_back(idx);
    }

    auto* gep = builder_->CreateGEP(allocType, alloca, gepIndices, varName + "_elem");

    // Determine element type by unwrapping array types
    llvm::Type* elemTy = allocType;
    for (size_t i = 0; i + 1 < exprs.size(); i++)
        elemTy = llvm::cast<llvm::ArrayType>(elemTy)->getElementType();

    // Cast value to element type if needed
    if (val->getType() != elemTy) {
        if (val->getType()->isIntegerTy() && elemTy->isIntegerTy())
            val = builder_->CreateIntCast(val, elemTy, elemTI->isSigned);
        else if (val->getType()->isFloatingPointTy() && elemTy->isFloatingPointTy()) {
            if (val->getType()->getPrimitiveSizeInBits() > elemTy->getPrimitiveSizeInBits())
                val = builder_->CreateFPTrunc(val, elemTy);
            else
                val = builder_->CreateFPExt(val, elemTy);
        }
    }

    builder_->CreateStore(val, gep);
    return {};
}



std::any IRGen::visitCompoundAssignStmt(LucisParser::CompoundAssignStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto varName = ctx->IDENTIFIER()->getText();
    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        std::cerr << "lucis: undefined variable '" << varName << "'\n";
        return {};
    }

    auto* alloca = it->second.alloca;
    auto* varTy  = alloca->getAllocatedType();
    auto* elemTI = it->second.typeInfo;

    auto* cur = builder_->CreateLoad(varTy, alloca, varName);
    auto* rhs = castValue(visit(ctx->expression()));

    // Cast RHS to variable type
    if (rhs->getType() != varTy) {
        if (rhs->getType()->isIntegerTy() && varTy->isIntegerTy())
            rhs = builder_->CreateIntCast(rhs, varTy, elemTI->isSigned);
        else if (rhs->getType()->isFloatingPointTy() && varTy->isFloatingPointTy()) {
            if (rhs->getType()->getPrimitiveSizeInBits() > varTy->getPrimitiveSizeInBits())
                rhs = builder_->CreateFPTrunc(rhs, varTy);
            else
                rhs = builder_->CreateFPExt(rhs, varTy);
        }
    }

    // ── Null-coalescing assignment: ptr ??= default ──────────────
    if (ctx->op->getType() == LucisLexer::NULLCOAL_ASSIGN) {
        auto* isNull = builder_->CreateICmpEQ(
            cur, llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_)),
            "ncoal_isnull");
        auto* thenBB = llvm::BasicBlock::Create(*context_, "ncoal.store", currentFunction_);
        auto* mergeBB = llvm::BasicBlock::Create(*context_, "ncoal.end", currentFunction_);
        builder_->CreateCondBr(isNull, thenBB, mergeBB);
        builder_->SetInsertPoint(thenBB);
        builder_->CreateStore(rhs, alloca);
        builder_->CreateBr(mergeBB);
        builder_->SetInsertPoint(mergeBB);
        return {};
    }

    bool isFloat = varTy->isFloatingPointTy();
    llvm::Value* result = nullptr;

    // ── Pointer arithmetic: ptr += int / ptr -= int ────────────
    bool isPtrArith = elemTI && elemTI->kind == TypeKind::Pointer &&
                      (ctx->op->getType() == LucisLexer::PLUS_ASSIGN ||
                       ctx->op->getType() == LucisLexer::MINUS_ASSIGN);
    if (isPtrArith) {
        auto* pointeeLLTy = elemTI->pointeeType
            ? elemTI->pointeeType->toLLVMType(*context_, module_->getDataLayout())
            : llvm::Type::getInt8Ty(*context_);
        auto* idx = rhs;
        if (ctx->op->getType() == LucisLexer::MINUS_ASSIGN)
            idx = builder_->CreateNeg(rhs, "neg");
        result = static_cast<llvm::Value*>(
            builder_->CreateGEP(pointeeLLTy, cur, idx, "ptr_cmpd"));
        builder_->CreateStore(result, alloca);
        return {};
    }

    // ── String concatenation: str += str ─────────────────────────
    if (ctx->op->getType() == LucisLexer::PLUS_ASSIGN &&
        elemTI && elemTI->kind == TypeKind::String) {
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* strTy   = varTy;

        auto* lhsPtr = builder_->CreateExtractValue(cur, {0}, "lhs.ptr");
        auto* lhsLen = builder_->CreateExtractValue(cur, {1}, "lhs.len");
        auto* rhsPtr = builder_->CreateExtractValue(rhs, {0}, "rhs.ptr");
        auto* rhsLen = builder_->CreateExtractValue(rhs, {1}, "rhs.len");

        auto callee = declareBuiltin("lucis_concat", strTy,
                                    {ptrTy, usizeTy, ptrTy, usizeTy});
        result = builder_->CreateCall(callee, {lhsPtr, lhsLen, rhsPtr, rhsLen}, "strcat");
        builder_->CreateStore(result, alloca);
        return {};
    }

    // Division by zero guard for /= and %=
    if (!isFloat && (ctx->op->getType() == LucisLexer::SLASH_ASSIGN ||
                     ctx->op->getType() == LucisLexer::PERCENT_ASSIGN)) {
        emitDivByZeroGuard(rhs, ctx->op);
    }

    switch (ctx->op->getType()) {
    case LucisLexer::PLUS_ASSIGN:
        result = isFloat ? builder_->CreateFAdd(cur, rhs) : builder_->CreateAdd(cur, rhs);
        break;
    case LucisLexer::MINUS_ASSIGN:
        result = isFloat ? builder_->CreateFSub(cur, rhs) : builder_->CreateSub(cur, rhs);
        break;
    case LucisLexer::STAR_ASSIGN:
        result = isFloat ? builder_->CreateFMul(cur, rhs) : builder_->CreateMul(cur, rhs);
        break;
    case LucisLexer::SLASH_ASSIGN:
        result = isFloat ? builder_->CreateFDiv(cur, rhs) : builder_->CreateSDiv(cur, rhs);
        break;
    case LucisLexer::PERCENT_ASSIGN:
        result = isFloat ? builder_->CreateFRem(cur, rhs) : builder_->CreateSRem(cur, rhs);
        break;
    case LucisLexer::AMP_ASSIGN:
        result = builder_->CreateAnd(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    case LucisLexer::PIPE_ASSIGN:
        result = builder_->CreateOr(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    case LucisLexer::CARET_ASSIGN:
        result = builder_->CreateXor(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    case LucisLexer::LSHIFT_ASSIGN:
        result = builder_->CreateShl(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    case LucisLexer::RSHIFT_ASSIGN:
        result = elemTI->isSigned ? builder_->CreateAShr(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs))
                                  : builder_->CreateLShr(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    default:
        result = cur;
        break;
    }

    builder_->CreateStore(result, alloca);
    return {};
}

// p.x = 42;


std::any IRGen::visitFieldAssignStmt(LucisParser::FieldAssignStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto identifiers = ctx->IDENTIFIER();
    auto varName = identifiers[0]->getText();

    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        std::cerr << "lucis: undefined variable '" << varName << "'\n";
        return {};
    }

    auto* alloca   = it->second.alloca;
    auto* structTI = it->second.typeInfo;
    auto* structTy = alloca->getAllocatedType();

    // ── Bitstruct field write: RMW with shift+mask ────────────────
    {
        const TypeInfo* bitTI = nullptr;
        llvm::Value* bitPtr = nullptr;
        if (structTI->kind == TypeKind::Pointer && structTI->pointeeType &&
            structTI->pointeeType->kind == TypeKind::BitStruct) {
            bitTI = structTI->pointeeType;
            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
            bitPtr = builder_->CreateLoad(ptrTy, alloca, varName + "_ptr");
        } else if (structTI->kind == TypeKind::BitStruct) {
            bitTI = structTI;
            bitPtr = alloca;
        }
        // Determine if volatile — check the pointer type's volatile qualifier
        bool isBitVol = structTI && structTI->isVolatile;

        if (bitTI && bitPtr && identifiers.size() == 2) {
            auto fieldName = identifiers[1]->getText();
            for (auto& bf : bitTI->bitFields) {
                if (bf.name == fieldName && !bf.isPadding) {
                    auto* rhs = castValue(visit(ctx->expression()));
                    auto* backTy = bitTI->backingType->toLLVMType(*context_, module_->getDataLayout());
                    auto* rawVal = builder_->CreateLoad(backTy, bitPtr, bitTI->name + "_raw");
                    if (isBitVol) llvm::cast<llvm::LoadInst>(rawVal)->setVolatile(true);
                    // Clear the field bits
                    uint64_t mask = ((1ULL << bf.width) - 1);
                    auto* clearMask = llvm::ConstantInt::get(backTy, ~(mask << bf.offset));
                    auto* cleared = builder_->CreateAnd(rawVal, clearMask, "bf_clear");
                    // Set the new value
                    auto* valExt = builder_->CreateZExt(rhs, backTy);
                    auto* shifted = valExt;
                    if (bf.offset > 0)
                        shifted = builder_->CreateShl(valExt,
                            llvm::ConstantInt::get(backTy, bf.offset), "bf_shl");
                    auto* result = builder_->CreateOr(cleared, shifted, "bf_set");
                    auto* store = builder_->CreateStore(result, bitPtr);
                    if (isBitVol) store->setVolatile(true);
                    return {};
                }
            }
        }
    }

    bool isVolatileAccess = false;
    if (structTI && structTI->kind == TypeKind::Pointer && structTI->pointeeType) {
        isVolatileAccess = structTI->pointeeType->isVolatile;
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        auto* basePtr = builder_->CreateLoad(ptrTy, alloca, varName + "_selfptr");
        alloca = static_cast<llvm::AllocaInst*>(nullptr);
        const TypeInfo* before = structTI;
        structTI = structTI->pointeeType;
        if (structTI && structTI->kind == TypeKind::Struct && structTI->fields.size() == 0)
            std::cerr << "DBG: pointeeType of '" << before->name << "' has 0 fields (name=" << structTI->name << ")\n";
        structTy = structTI->toLLVMType(*context_, module_->getDataLayout());

        // Walk field chain on pointer base.
        llvm::Value* currentPtr = basePtr;
        llvm::Type* currentTy = structTy;
        const TypeInfo* currentTI = structTI;
        for (size_t i = 1; i < identifiers.size(); i++) {
            auto fieldName = identifiers[i]->getText();
    // Auto-deref pointer for chained field access
    if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
        auto* derefPtrTy = llvm::PointerType::getUnqual(*context_);
        currentPtr = builder_->CreateLoad(derefPtrTy, currentPtr, "chain_deref");
        currentTI = currentTI->pointeeType;
        currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
    }
            int fieldIdx = -1;
            for (size_t f = 0; f < currentTI->fields.size(); f++) {
                if (currentTI->fields[f].name == fieldName) {
                    fieldIdx = static_cast<int>(f);
                    break;
                }
            }
            if (fieldIdx < 0) {
                std::cerr << "lucis: '" << currentTI->name
                          << "' has no field '" << fieldName << "'";
                if (currentTI) std::cerr << " (fields=" << currentTI->fields.size() << " kind=" << (int)currentTI->kind << ")";
                std::cerr << "\n";
                return {};
            }

            if (currentTI->kind == TypeKind::Union) {
                currentTI = currentTI->fields[fieldIdx].typeInfo;
                currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
            } else {
                currentPtr = builder_->CreateStructGEP(currentTy, currentPtr,
                                                        fieldIdx, fieldName + "_ptr");
                currentTI = currentTI->fields[fieldIdx].typeInfo;
                currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
            }
        }

        auto* val = castValue(visit(ctx->expression()));
        if (val->getType() != currentTy) {
            if (val->getType()->isIntegerTy() && currentTy->isIntegerTy())
                val = builder_->CreateIntCast(val, currentTy, currentTI->isSigned);
            else if (val->getType()->isFloatingPointTy() && currentTy->isFloatingPointTy()) {
                if (val->getType()->getPrimitiveSizeInBits() > currentTy->getPrimitiveSizeInBits())
                    val = builder_->CreateFPTrunc(val, currentTy);
                else
                    val = builder_->CreateFPExt(val, currentTy);
            }
        }

        if (isVolatileAccess)
            builder_->CreateStore(val, currentPtr, true);
        else
            builder_->CreateStore(val, currentPtr);

        // Ownership: the field now owns the stored value — consume the RHS
        // local so its auto-cleanup doesn't free the same buffer.
        if (currentTI && isOwningFieldType(currentTI))
            consumeExprIfOwnedLocal(ctx->expression());
        return {};
    }

    // Walk field chain: p.x.y → GEP(0, fieldIdx_x), then GEP(0, fieldIdx_y)
    llvm::Value* currentPtr = alloca;
    llvm::Type*  currentTy  = structTy;
    const TypeInfo* currentTI = structTI;
    const FieldInfo* lastField = nullptr;

    for (size_t i = 1; i < identifiers.size(); i++) {
        auto fieldName = identifiers[i]->getText();
    // Auto-deref pointer for chained field access
    if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
        auto* derefPtrTy = llvm::PointerType::getUnqual(*context_);
        currentPtr = builder_->CreateLoad(derefPtrTy, currentPtr, "chain_deref");
        currentTI = currentTI->pointeeType;
        currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
    }
        int fieldIdx = -1;
        for (size_t f = 0; f < currentTI->fields.size(); f++) {
            if (currentTI->fields[f].name == fieldName) {
                fieldIdx = static_cast<int>(f);
                break;
            }
        }
        if (fieldIdx < 0) {
            // BitStruct field in chain: atc.reg.counter — handle bitfield RMW
            if (currentTI && currentTI->kind == TypeKind::BitStruct) {
                for (auto& bf : currentTI->bitFields) {
                    if (bf.name == fieldName && !bf.isPadding) {
                        auto* val = castValue(visit(ctx->expression()));
                        auto* backTy = currentTI->backingType->toLLVMType(*context_, module_->getDataLayout());
                        auto* rawVal = builder_->CreateLoad(backTy, currentPtr, currentTI->name + "_raw");
                        uint64_t mask = ((1ULL << bf.width) - 1);
                        auto* cleared = builder_->CreateAnd(rawVal,
                            llvm::ConstantInt::get(backTy, ~(mask << bf.offset)));
                        auto* valExt = builder_->CreateZExt(val, backTy);
                        auto* shifted = bf.offset > 0
                            ? builder_->CreateShl(valExt, llvm::ConstantInt::get(backTy, bf.offset))
                            : valExt;
                        auto* result = builder_->CreateOr(cleared, shifted);
                        builder_->CreateStore(result, currentPtr);
                        return {};
                    }
                }
            }
            std::cerr << "lucis: '" << currentTI->name
                      << "' has no field '" << fieldName << "'\n";
            return {};
        }

        lastField = &currentTI->fields[fieldIdx];
        if (currentTI->kind == TypeKind::Union) {
            // Union: all fields at offset 0 — pointer stays the same
            currentTI = currentTI->fields[fieldIdx].typeInfo;
            currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
        } else {
            currentPtr = builder_->CreateStructGEP(currentTy, currentPtr,
                                                     fieldIdx, fieldName + "_ptr");
            currentTI = currentTI->fields[fieldIdx].typeInfo;
            currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
        }
    }

    auto* val = castValue(visit(ctx->expression()));

    // Array-to-slice lowering for dynamic array fields
    if (lastField && lastField->arrayDims > 0 && lastField->arraySizes.empty() &&
        val->getType()->isArrayTy()) {
        auto* sliceTy = buildFieldLLVMType(lastField->typeInfo,
                                           lastField->arrayDims,
                                           lastField->arraySizes);
        if (sliceTy->isStructTy() &&
            llvm::cast<llvm::StructType>(sliceTy)->getNumElements() == 2) {
            val = lowerArrayToSlice(val, sliceTy);
        }
    }

    // Cast if needed
    if (val->getType() != currentTy) {
        if (val->getType()->isIntegerTy() && currentTy->isIntegerTy())
            val = builder_->CreateIntCast(val, currentTy, currentTI->isSigned);
        else if (val->getType()->isFloatingPointTy() && currentTy->isFloatingPointTy()) {
            if (val->getType()->getPrimitiveSizeInBits() > currentTy->getPrimitiveSizeInBits())
                val = builder_->CreateFPTrunc(val, currentTy);
            else
                val = builder_->CreateFPExt(val, currentTy);
        }
    }

    // BitStruct field write through chained access: atc.reg.counter = 42
    if (identifiers.size() > 2 && structTI->kind != TypeKind::BitStruct) {
        // Check if the LAST identifier accesses a bitstruct field
        auto* baseType = it->second.typeInfo;
        if (baseType && baseType->kind == TypeKind::Pointer && baseType->pointeeType)
            baseType = baseType->pointeeType;
        const TypeInfo* bitTI = nullptr;
        llvm::Value* bitPtr = nullptr;
        for (size_t i = 1; i < identifiers.size() - 1 && baseType; i++) {
            if (baseType->kind == TypeKind::BitStruct) { bitTI = baseType; break; }
            if (baseType->kind == TypeKind::Struct || baseType->kind == TypeKind::Union) {
                bool found = false;
                for (auto& f : baseType->fields) {
                    if (f.name == identifiers[i]->getText()) {
                        baseType = f.typeInfo;
                        found = true;
                        break;
                    }
                }
                if (!found) break;
            } else break;
        }
        if (!bitTI && baseType && baseType->kind == TypeKind::BitStruct)
            bitTI = baseType;
        if (bitTI) {
            auto fieldName = identifiers.back()->getText();
            for (auto& bf : bitTI->bitFields) {
                if (bf.name == fieldName && !bf.isPadding) {
                    auto* backTy = bitTI->backingType->toLLVMType(*context_, module_->getDataLayout());
                    auto* rawVal = builder_->CreateLoad(backTy, currentPtr, bitTI->name + "_raw");
                    uint64_t mask = ((1ULL << bf.width) - 1);
                    auto* cleared = builder_->CreateAnd(rawVal,
                        llvm::ConstantInt::get(backTy, ~(mask << bf.offset)));
                    auto* valExt = builder_->CreateZExt(val, backTy);
                    auto* shifted = valExt;
                    if (bf.offset > 0)
                        shifted = builder_->CreateShl(valExt,
                            llvm::ConstantInt::get(backTy, bf.offset));
                    auto* result = builder_->CreateOr(cleared, shifted);
                    builder_->CreateStore(result, currentPtr);
                    return {};
                }
            }
        }
    }
    if (isVolatileAccess || (currentTI && currentTI->isVolatile))
        builder_->CreateStore(val, currentPtr, true);
    else
        builder_->CreateStore(val, currentPtr);

    // Ownership: the field now owns the stored value — consume the RHS
    // local so its auto-cleanup doesn't free the same buffer.
    if (currentTI && isOwningFieldType(currentTI))
        consumeExprIfOwnedLocal(ctx->expression());
    return {};
}

// p.x += 5;  data.algo.pos.x -= 10;


std::any IRGen::visitFieldCompoundAssignStmt(
    LucisParser::FieldCompoundAssignStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto identifiers = ctx->IDENTIFIER();
    auto varName = identifiers[0]->getText();

    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        std::cerr << "lucis: undefined variable '" << varName << "'\n";
        return {};
    }

    auto* alloca   = it->second.alloca;
    auto* structTI = it->second.typeInfo;
    auto* structTy = alloca->getAllocatedType();

    // ── Bitstruct field compound assign (RMW with op) ────────────
    {
        const TypeInfo* bitTI = nullptr;
        llvm::Value* bitPtr = nullptr;
        if (structTI->kind == TypeKind::Pointer && structTI->pointeeType &&
            structTI->pointeeType->kind == TypeKind::BitStruct) {
            bitTI = structTI->pointeeType;
            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
            bitPtr = builder_->CreateLoad(ptrTy, alloca, varName + "_ptr");
        } else if (structTI->kind == TypeKind::BitStruct) {
            bitTI = structTI;
            bitPtr = alloca;
        }
        if (bitTI && bitPtr && identifiers.size() == 2) {
            auto fieldName = identifiers[1]->getText();
            for (auto& bf : bitTI->bitFields) {
                if (bf.name == fieldName && !bf.isPadding) {
                    bool isBitVol = structTI && structTI->isVolatile;
                    auto* backTy = bitTI->backingType->toLLVMType(*context_, module_->getDataLayout());
                    // Load raw value
                    auto* rawVal = builder_->CreateLoad(backTy, bitPtr, bitTI->name + "_raw");
                    if (isBitVol) llvm::cast<llvm::LoadInst>(rawVal)->setVolatile(true);
                    // Extract current field value
                    uint64_t mask = (1ULL << bf.width) - 1;
                    llvm::Value* curVal = rawVal;
                    if (bf.offset > 0)
                        curVal = builder_->CreateLShr(rawVal, llvm::ConstantInt::get(backTy, bf.offset));
                    curVal = builder_->CreateAnd(curVal, llvm::ConstantInt::get(backTy, mask));
                    // Evaluate RHS and apply operator
                    auto* rhs = castValue(visit(ctx->expression()));
                    auto* rhsExt = builder_->CreateZExt(rhs, backTy);
                    llvm::Value* newVal = nullptr;
                    switch (ctx->op->getType()) {
                    case LucisLexer::PLUS_ASSIGN:  newVal = builder_->CreateAdd(curVal, rhsExt); break;
                    case LucisLexer::MINUS_ASSIGN: newVal = builder_->CreateSub(curVal, rhsExt); break;
                    case LucisLexer::STAR_ASSIGN:  newVal = builder_->CreateMul(curVal, rhsExt); break;
                    case LucisLexer::AMP_ASSIGN:   newVal = builder_->CreateAnd(curVal, rhsExt); break;
                    case LucisLexer::PIPE_ASSIGN:  newVal = builder_->CreateOr(curVal, rhsExt);  break;
                    case LucisLexer::CARET_ASSIGN: newVal = builder_->CreateXor(curVal, rhsExt); break;
                    default:
                        std::cerr << "lucis: unsupported compound assign on bitstruct field\n";
                        return {};
                    }
                    // Clear and set field
                    auto* cleared = builder_->CreateAnd(rawVal,
                        llvm::ConstantInt::get(backTy, ~(mask << bf.offset)));
                    auto* shifted = newVal;
                    if (bf.offset > 0)
                        shifted = builder_->CreateShl(newVal, llvm::ConstantInt::get(backTy, bf.offset));
                    auto* result = builder_->CreateOr(cleared, shifted, "bf_cmpd");
                    auto* store = builder_->CreateStore(result, bitPtr);
                    if (isBitVol) store->setVolatile(true);
                    return {};
                }
            }
        }
    }

    if (structTI && structTI->kind == TypeKind::Pointer && structTI->pointeeType) {
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        auto* basePtr = builder_->CreateLoad(ptrTy, alloca, varName + "_selfptr");
        structTI = structTI->pointeeType;
        structTy = structTI->toLLVMType(*context_, module_->getDataLayout());

        llvm::Value* currentPtr = basePtr;
        llvm::Type* currentTy = structTy;
        const TypeInfo* currentTI = structTI;

        for (size_t i = 1; i < identifiers.size(); i++) {
            auto fieldName = identifiers[i]->getText();
    // Auto-deref pointer for chained field access
    if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
        auto* derefPtrTy = llvm::PointerType::getUnqual(*context_);
        currentPtr = builder_->CreateLoad(derefPtrTy, currentPtr, "chain_deref");
        currentTI = currentTI->pointeeType;
        currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
    }
            int fieldIdx = -1;
            for (size_t f = 0; f < currentTI->fields.size(); f++) {
                if (currentTI->fields[f].name == fieldName) {
                    fieldIdx = static_cast<int>(f);
                    break;
                }
            }
            if (fieldIdx < 0) {
                std::cerr << "lucis: '" << currentTI->name
                          << "' has no field '" << fieldName << "'\n";
                return {};
            }

            if (currentTI->kind == TypeKind::Union) {
                currentTI = currentTI->fields[fieldIdx].typeInfo;
                currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
            } else {
                currentPtr = builder_->CreateStructGEP(currentTy, currentPtr,
                                                       fieldIdx, fieldName + "_ptr");
                currentTI = currentTI->fields[fieldIdx].typeInfo;
                currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
            }
        }

        auto* cur = builder_->CreateLoad(currentTy, currentPtr, "field_cur");
        if (currentTI->isVolatile)
            llvm::cast<llvm::LoadInst>(cur)->setVolatile(true);
        auto* rhs = castValue(visit(ctx->expression()));
        if (rhs->getType() != currentTy) {
            if (rhs->getType()->isIntegerTy() && currentTy->isIntegerTy())
                rhs = builder_->CreateIntCast(rhs, currentTy, currentTI->isSigned);
            else if (rhs->getType()->isFloatingPointTy() && currentTy->isFloatingPointTy()) {
                if (rhs->getType()->getPrimitiveSizeInBits() > currentTy->getPrimitiveSizeInBits())
                    rhs = builder_->CreateFPTrunc(rhs, currentTy);
                else
                    rhs = builder_->CreateFPExt(rhs, currentTy);
            }
        }

        auto volatileStore = [&](llvm::Value* v, llvm::Value* p) {
            auto* s = builder_->CreateStore(v, p);
            if (currentTI->isVolatile) s->setVolatile(true);
        };

        // ── Null-coalescing: ptr ??= default ──────────────────
        if (ctx->op->getType() == LucisLexer::NULLCOAL_ASSIGN) {
            auto* isNull = builder_->CreateICmpEQ(
                cur, llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_)),
                "ncoal_isnull");
            auto* thenBB = llvm::BasicBlock::Create(*context_, "ncoal.store", currentFunction_);
            auto* mergeBB = llvm::BasicBlock::Create(*context_, "ncoal.end", currentFunction_);
            builder_->CreateCondBr(isNull, thenBB, mergeBB);
            builder_->SetInsertPoint(thenBB);
            volatileStore(rhs, currentPtr);
            builder_->CreateBr(mergeBB);
            builder_->SetInsertPoint(mergeBB);
            return {};
        }

        bool isFloat = currentTy->isFloatingPointTy();
        llvm::Value* result = nullptr;
        // Pointer arithmetic on pointer field: field += int / field -= int
        if (currentTI && currentTI->kind == TypeKind::Pointer &&
            (ctx->op->getType() == LucisLexer::PLUS_ASSIGN ||
             ctx->op->getType() == LucisLexer::MINUS_ASSIGN)) {
            auto* pointeeLLTy = currentTI->pointeeType
                ? currentTI->pointeeType->toLLVMType(*context_, module_->getDataLayout())
                : llvm::Type::getInt8Ty(*context_);
            auto* idx = rhs;
            if (ctx->op->getType() == LucisLexer::MINUS_ASSIGN)
                idx = builder_->CreateNeg(rhs, "neg");
            result = static_cast<llvm::Value*>(
                builder_->CreateGEP(pointeeLLTy, cur, idx, "ptr_cmpd"));
            volatileStore(result, currentPtr);
            return {};
        }
        if (!isFloat && (ctx->op->getType() == LucisLexer::SLASH_ASSIGN ||
                         ctx->op->getType() == LucisLexer::PERCENT_ASSIGN)) {
            emitDivByZeroGuard(rhs, ctx->op);
        }
        switch (ctx->op->getType()) {
        case LucisLexer::PLUS_ASSIGN:
            result = isFloat ? builder_->CreateFAdd(cur, rhs) : builder_->CreateAdd(cur, rhs);
            break;
        case LucisLexer::MINUS_ASSIGN:
            result = isFloat ? builder_->CreateFSub(cur, rhs) : builder_->CreateSub(cur, rhs);
            break;
        case LucisLexer::STAR_ASSIGN:
            result = isFloat ? builder_->CreateFMul(cur, rhs) : builder_->CreateMul(cur, rhs);
            break;
        case LucisLexer::SLASH_ASSIGN:
            result = isFloat ? builder_->CreateFDiv(cur, rhs) : builder_->CreateSDiv(cur, rhs);
            break;
        case LucisLexer::PERCENT_ASSIGN:
            result = isFloat ? builder_->CreateFRem(cur, rhs) : builder_->CreateSRem(cur, rhs);
            break;
        case LucisLexer::AMP_ASSIGN:
            result = builder_->CreateAnd(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
            break;
        case LucisLexer::PIPE_ASSIGN:
            result = builder_->CreateOr(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
            break;
        case LucisLexer::CARET_ASSIGN:
            result = builder_->CreateXor(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
            break;
        case LucisLexer::LSHIFT_ASSIGN:
            result = builder_->CreateShl(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
            break;
        case LucisLexer::RSHIFT_ASSIGN:
            result = currentTI->isSigned ? builder_->CreateAShr(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs))
                                         : builder_->CreateLShr(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
            break;
        default:
            result = cur;
            break;
        }
        volatileStore(result, currentPtr);
        return {};
    }

    // Walk field chain: p.x.y → GEP(0, fieldIdx_x), then GEP(0, fieldIdx_y)
    llvm::Value* currentPtr = alloca;
    llvm::Type*  currentTy  = structTy;
    const TypeInfo* currentTI = structTI;

    for (size_t i = 1; i < identifiers.size(); i++) {
        auto fieldName = identifiers[i]->getText();
    // Auto-deref pointer for chained field access
    if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
        auto* derefPtrTy = llvm::PointerType::getUnqual(*context_);
        currentPtr = builder_->CreateLoad(derefPtrTy, currentPtr, "chain_deref");
        currentTI = currentTI->pointeeType;
        currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
    }
        int fieldIdx = -1;
        for (size_t f = 0; f < currentTI->fields.size(); f++) {
            if (currentTI->fields[f].name == fieldName) {
                fieldIdx = static_cast<int>(f);
                break;
            }
        }
        if (fieldIdx < 0) {
            std::cerr << "lucis: '" << currentTI->name
                      << "' has no field '" << fieldName << "'\n";
            return {};
        }

        if (currentTI->kind == TypeKind::Union) {
            currentTI = currentTI->fields[fieldIdx].typeInfo;
            currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
        } else {
            currentPtr = builder_->CreateStructGEP(currentTy, currentPtr,
                                                   fieldIdx, fieldName + "_ptr");
            currentTI = currentTI->fields[fieldIdx].typeInfo;
            currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
        }
    }

    auto* cur = builder_->CreateLoad(currentTy, currentPtr, "field_cur");
    if (currentTI->isVolatile)
        llvm::cast<llvm::LoadInst>(cur)->setVolatile(true);
    auto* rhs = castValue(visit(ctx->expression()));

    auto nvStore = [&](llvm::Value* v, llvm::Value* p) {
        auto* s = builder_->CreateStore(v, p);
        if (currentTI->isVolatile) s->setVolatile(true);
    };

    // Cast RHS to field type
    if (rhs->getType() != currentTy) {
        if (rhs->getType()->isIntegerTy() && currentTy->isIntegerTy())
            rhs = builder_->CreateIntCast(rhs, currentTy, currentTI->isSigned);
        else if (rhs->getType()->isFloatingPointTy() && currentTy->isFloatingPointTy()) {
            if (rhs->getType()->getPrimitiveSizeInBits() > currentTy->getPrimitiveSizeInBits())
                rhs = builder_->CreateFPTrunc(rhs, currentTy);
            else
                rhs = builder_->CreateFPExt(rhs, currentTy);
        }
    }

    // ── Null-coalescing: ptr ??= default ──────────────────
    if (ctx->op->getType() == LucisLexer::NULLCOAL_ASSIGN) {
        auto* isNull = builder_->CreateICmpEQ(
            cur, llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_)),
            "ncoal_isnull");
        auto* thenBB = llvm::BasicBlock::Create(*context_, "ncoal.store", currentFunction_);
        auto* mergeBB = llvm::BasicBlock::Create(*context_, "ncoal.end", currentFunction_);
        builder_->CreateCondBr(isNull, thenBB, mergeBB);
        builder_->SetInsertPoint(thenBB);
        nvStore(rhs, currentPtr);
        builder_->CreateBr(mergeBB);
        builder_->SetInsertPoint(mergeBB);
        return {};
    }

    bool isFloat = currentTy->isFloatingPointTy();
    llvm::Value* result = nullptr;

    // Pointer arithmetic on pointer field: field += int / field -= int
    if (currentTI && currentTI->kind == TypeKind::Pointer &&
        (ctx->op->getType() == LucisLexer::PLUS_ASSIGN ||
         ctx->op->getType() == LucisLexer::MINUS_ASSIGN)) {
        auto* pointeeLLTy = currentTI->pointeeType
            ? currentTI->pointeeType->toLLVMType(*context_, module_->getDataLayout())
            : llvm::Type::getInt8Ty(*context_);
        auto* idx = rhs;
        if (ctx->op->getType() == LucisLexer::MINUS_ASSIGN)
            idx = builder_->CreateNeg(rhs, "neg");
        result = static_cast<llvm::Value*>(
            builder_->CreateGEP(pointeeLLTy, cur, idx, "ptr_cmpd"));
        nvStore(result, currentPtr);
        return {};
    }

    // Division by zero guard for /= and %=
    if (!isFloat && (ctx->op->getType() == LucisLexer::SLASH_ASSIGN ||
                     ctx->op->getType() == LucisLexer::PERCENT_ASSIGN)) {
        emitDivByZeroGuard(rhs, ctx->op);
    }

    switch (ctx->op->getType()) {
    case LucisLexer::PLUS_ASSIGN:
        result = isFloat ? builder_->CreateFAdd(cur, rhs) : builder_->CreateAdd(cur, rhs);
        break;
    case LucisLexer::MINUS_ASSIGN:
        result = isFloat ? builder_->CreateFSub(cur, rhs) : builder_->CreateSub(cur, rhs);
        break;
    case LucisLexer::STAR_ASSIGN:
        result = isFloat ? builder_->CreateFMul(cur, rhs) : builder_->CreateMul(cur, rhs);
        break;
    case LucisLexer::SLASH_ASSIGN:
        result = isFloat ? builder_->CreateFDiv(cur, rhs) : builder_->CreateSDiv(cur, rhs);
        break;
    case LucisLexer::PERCENT_ASSIGN:
        result = isFloat ? builder_->CreateFRem(cur, rhs) : builder_->CreateSRem(cur, rhs);
        break;
    case LucisLexer::AMP_ASSIGN:
        result = builder_->CreateAnd(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    case LucisLexer::PIPE_ASSIGN:
        result = builder_->CreateOr(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    case LucisLexer::CARET_ASSIGN:
        result = builder_->CreateXor(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    case LucisLexer::LSHIFT_ASSIGN:
        result = builder_->CreateShl(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    case LucisLexer::RSHIFT_ASSIGN:
        result = currentTI->isSigned ? builder_->CreateAShr(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs))
                                     : builder_->CreateLShr(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    default:
        result = cur;
        break;
    }

    nvStore(result, currentPtr);
    return {};
}

// arr[i].field = value;  or  arr[i][j].field.subfield = value;


std::any IRGen::visitIndexFieldAssignStmt(
    LucisParser::IndexFieldAssignStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto identifiers = ctx->IDENTIFIER();
    auto varName = identifiers[0]->getText();

    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        std::cerr << "lucis: undefined variable '" << varName << "'\n";
        return {};
    }

    auto* alloca   = it->second.alloca;
    auto* allocType = alloca->getAllocatedType();
    auto* structTI = it->second.typeInfo;
    auto* i64Ty    = llvm::Type::getInt64Ty(*context_);

    // The number of index expressions equals the number of '[' tokens
    size_t numIndices = ctx->LBRACKET().size();
    auto expressions = ctx->expression();
    // expressions layout: [index0, index1, ..., indexN-1, rhs_value]
    // The last expression is always the value to assign

    llvm::Value* elemPtr = nullptr;
    llvm::Type* elemTy = nullptr;
    const TypeInfo* currentTI = structTI;

    if (structTI && structTI->kind == TypeKind::Pointer && structTI->pointeeType) {
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        auto* basePtr = builder_->CreateLoad(ptrTy, alloca, varName + "_ptr");

        elemTy = structTI->pointeeType->toLLVMType(*context_, module_->getDataLayout());
        currentTI = structTI->pointeeType;
        elemPtr = basePtr;

        for (size_t i = 0; i < numIndices; i++) {
            auto* idx = castValue(visit(expressions[i]));
            if (idx->getType() != i64Ty)
                idx = builder_->CreateIntCast(idx, i64Ty, true);

            if (auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(elemTy)) {
                elemPtr = builder_->CreateInBoundsGEP(
                    arrTy,
                    elemPtr,
                    { llvm::ConstantInt::get(i64Ty, 0), idx },
                    varName + "_elem_ptr");
                elemTy = arrTy->getElementType();
            } else {
                if (i > 0) {
                    std::cerr << "lucis: too many indices for pointer variable '"
                              << varName << "'\n";
                    return {};
                }
                elemPtr = builder_->CreateInBoundsGEP(
                    elemTy,
                    elemPtr,
                    idx,
                    varName + "_elem_ptr");
            }
        }
    } else {
        // Build GEP indices for array: [0, idx0, idx1, ...]
        std::vector<llvm::Value*> gepIndices;
        gepIndices.push_back(llvm::ConstantInt::get(i64Ty, 0));
        for (size_t i = 0; i < numIndices; i++) {
            auto* idx = castValue(visit(expressions[i]));
            if (idx->getType() != i64Ty)
                idx = builder_->CreateIntCast(idx, i64Ty, true);
            gepIndices.push_back(idx);
        }

        // GEP to the array element (a struct)
        elemPtr = builder_->CreateGEP(allocType, alloca, gepIndices,
                                      varName + "_elem_ptr");

        // Determine element type by unwrapping array dimensions
        elemTy = allocType;
        for (size_t i = 0; i < numIndices; i++) {
            auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(elemTy);
            if (!arrTy) {
                std::cerr << "lucis: too many indices for array variable '"
                          << varName << "'\n";
                return {};
            }
            elemTy = arrTy->getElementType();
        }
    }

    // Walk the field chain: identifiers[1], identifiers[2], ...
    llvm::Value* currentPtr = elemPtr;
    llvm::Type*  currentTy  = elemTy;

    if (currentTI && currentTI->kind == TypeKind::Pointer &&
        currentTI->pointeeType) {
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        currentPtr = builder_->CreateLoad(ptrTy, currentPtr, varName + "_elem_load");
        currentTI = currentTI->pointeeType;
        currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
    }

    for (size_t i = 1; i < identifiers.size(); i++) {
        auto fieldName = identifiers[i]->getText();
    // Auto-deref pointer for chained field access
    if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
        auto* derefPtrTy = llvm::PointerType::getUnqual(*context_);
        currentPtr = builder_->CreateLoad(derefPtrTy, currentPtr, "chain_deref");
        currentTI = currentTI->pointeeType;
        currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
    }
        int fieldIdx = -1;
        for (size_t f = 0; f < currentTI->fields.size(); f++) {
            if (currentTI->fields[f].name == fieldName) {
                fieldIdx = static_cast<int>(f);
                break;
            }
        }
        if (fieldIdx < 0) {
            std::cerr << "lucis: '" << currentTI->name
                      << "' has no field '" << fieldName << "'\n";
            return {};
        }

        if (currentTI->kind == TypeKind::Union) {
            currentTI = currentTI->fields[fieldIdx].typeInfo;
            currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
        } else {
            currentPtr = builder_->CreateStructGEP(currentTy, currentPtr,
                                                    fieldIdx, fieldName + "_ptr");
            currentTI = currentTI->fields[fieldIdx].typeInfo;
            currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
        }
    }

    // The RHS value is the last expression
    auto* val = castValue(visit(expressions[expressions.size() - 1]));

    // Cast if needed
    if (val->getType() != currentTy) {
        if (val->getType()->isIntegerTy() && currentTy->isIntegerTy())
            val = builder_->CreateIntCast(val, currentTy, currentTI->isSigned);
        else if (val->getType()->isFloatingPointTy() && currentTy->isFloatingPointTy()) {
            if (val->getType()->getPrimitiveSizeInBits() > currentTy->getPrimitiveSizeInBits())
                val = builder_->CreateFPTrunc(val, currentTy);
            else
                val = builder_->CreateFPExt(val, currentTy);
        }
    }

    {
        auto* store = builder_->CreateStore(val, currentPtr);
        if (currentTI && currentTI->isVolatile) store->setVolatile(true);
    }
    return {};
}

// obj.field[i] = value;   ts.buf[3] = t;


void IRGen::emitExtendedIndexAssign(
    llvm::Value* containerPtr, const TypeInfo* extTI,
    const std::vector<LucisParser::ExpressionContext*>& expressions) {
    auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
    auto suffix   = extTI->builtinSuffix;

    if (extTI->extendedKind == "Map") {
        // Map<K,V> subscript assignment: m[key] = val → lucis_map_set_<suffix>
        auto* keyLLTy = extTI->keyType->toLLVMType(*context_, module_->getDataLayout());
        auto* valLLTy = extTI->valueType->toLLVMType(*context_, module_->getDataLayout());

        auto* keyVal = castValue(visit(expressions[0]));
        if (keyVal->getType() != keyLLTy) {
            if (keyVal->getType()->isIntegerTy() && keyLLTy->isIntegerTy())
                keyVal = builder_->CreateIntCast(keyVal, keyLLTy, extTI->keyType->isSigned);
        }

        auto* val = castValue(visit(expressions[expressions.size() - 1]));

        // Handle raw value types: pass by pointer
        bool isRawVal = suffix.size() > 4 && suffix.substr(suffix.size() - 3) == "raw";

        if (isRawVal) {
            auto* valAlloca = builder_->CreateAlloca(valLLTy, nullptr, "map_set_val");
            builder_->CreateStore(val, valAlloca);
            auto callee = declareBuiltin(
                "lucis_map_set_" + suffix,
                llvm::Type::getVoidTy(*context_),
                { ptrTy, keyLLTy, ptrTy });
            builder_->CreateCall(callee, { containerPtr, keyVal, valAlloca });
        } else {
            if (val->getType() != valLLTy) {
                if (val->getType()->isIntegerTy() && valLLTy->isIntegerTy())
                    val = builder_->CreateIntCast(val, valLLTy, extTI->valueType->isSigned);
                else if (val->getType()->isFloatingPointTy() && valLLTy->isFloatingPointTy()) {
                    if (val->getType()->getPrimitiveSizeInBits() > valLLTy->getPrimitiveSizeInBits())
                        val = builder_->CreateFPTrunc(val, valLLTy);
                    else
                        val = builder_->CreateFPExt(val, valLLTy);
                }
            }
            auto callee = declareBuiltin(
                "lucis_map_set_" + suffix,
                llvm::Type::getVoidTy(*context_),
                { ptrTy, keyLLTy, valLLTy });
            builder_->CreateCall(callee, { containerPtr, keyVal, val });
        }
        return;
    }

    // Vec<T> or Set<T> index assignment
    auto* elemLLTy = extTI->elementType->toLLVMType(*context_, module_->getDataLayout());
    auto vecSuffix = getVecSuffix(extTI->elementType);

    auto* idx = castValue(visit(expressions[0]));
    if (idx->getType() != usizeTy)
        idx = builder_->CreateIntCast(idx, usizeTy, false);

    auto* val = castValue(visit(expressions[expressions.size() - 1]));
    if (val->getType() != elemLLTy) {
        if (val->getType()->isIntegerTy() && elemLLTy->isIntegerTy())
            val = builder_->CreateIntCast(val, elemLLTy, extTI->elementType->isSigned);
        else if (val->getType()->isFloatingPointTy() && elemLLTy->isFloatingPointTy()) {
            if (val->getType()->getPrimitiveSizeInBits() > elemLLTy->getPrimitiveSizeInBits())
                val = builder_->CreateFPTrunc(val, elemLLTy);
            else
                val = builder_->CreateFPExt(val, elemLLTy);
        }
    }

    if (extTI->extendedKind == "Set") {
        auto callee = declareBuiltin(
            "lucis_set_add_" + suffix,
            llvm::Type::getInt32Ty(*context_),
            { ptrTy, elemLLTy });
        builder_->CreateCall(callee, { containerPtr, val });
        return;
    }

    // Vec<T> index assignment: v[i] = x → lucis_vec_set_<suffix>
    if (vecSuffix == "raw") {
        auto& dl     = module_->getDataLayout();
        auto  elemSz = dl.getTypeAllocSize(elemLLTy);
        auto* elemSzVal = llvm::ConstantInt::get(usizeTy, elemSz);
        auto ptrFn = declareBuiltin("lucis_vec_ptr_raw", ptrTy,
                                    {ptrTy, usizeTy, usizeTy});
        auto* elemPtr = builder_->CreateCall(ptrFn,
            { containerPtr, idx, elemSzVal }, "vec_elem_ptr");
        builder_->CreateStore(val, elemPtr);
        return;
    }

    auto callee = declareBuiltin(
        "lucis_vec_set_" + vecSuffix,
        llvm::Type::getVoidTy(*context_),
        { ptrTy, usizeTy, elemLLTy });
    builder_->CreateCall(callee, { containerPtr, idx, val });
}

std::any IRGen::visitFieldIndexAssignStmt(LucisParser::FieldIndexAssignStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto identifiers = ctx->IDENTIFIER();
    auto varName = identifiers[0]->getText();

    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        std::cerr << "lucis: undefined variable '" << varName << "'\n";
        return {};
    }

    auto* alloca    = it->second.alloca;
    auto* allocType = alloca->getAllocatedType();
    auto* i64Ty     = llvm::Type::getInt64Ty(*context_);

    // Walk field chain: identifiers[1], identifiers[2], ...
    llvm::Value*    currentPtr = alloca;
    llvm::Type*     currentTy  = allocType;
    const TypeInfo* currentTI  = it->second.typeInfo;

    // If the base is a pointer, load it first
    if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        currentPtr = builder_->CreateLoad(ptrTy, alloca, varName + "_ptr");
        currentTI  = currentTI->pointeeType;
        currentTy  = currentTI->toLLVMType(*context_, module_->getDataLayout());
    }

    for (size_t i = 1; i < identifiers.size(); i++) {
        auto fieldName = identifiers[i]->getText();

        if (!currentTI || (currentTI->kind != TypeKind::Struct &&
                           currentTI->kind != TypeKind::Union)) {
            std::cerr << "lucis: cannot access field '" << fieldName
                      << "' on non-struct type\n";
            return {};
        }

        int fieldIdx = -1;
        for (size_t f = 0; f < currentTI->fields.size(); f++) {
            if (currentTI->fields[f].name == fieldName) {
                fieldIdx = static_cast<int>(f);
                break;
            }
        }
        if (fieldIdx < 0) {
            std::cerr << "lucis: '" << currentTI->name
                      << "' has no field '" << fieldName << "'\n";
            return {};
        }

        if (currentTI->kind == TypeKind::Union) {
            // Union: reinterpret pointer, type changes but address stays
            currentTI = currentTI->fields[fieldIdx].typeInfo;
            currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
            // currentPtr stays the same (union shares memory)
        } else {
            const auto& fi = currentTI->fields[fieldIdx];
            currentPtr = builder_->CreateStructGEP(currentTy, currentPtr,
                                                    fieldIdx, fieldName + "_ptr");
            currentTI  = fi.typeInfo;
            // Build the correct LLVM type for the field (may be an array)
            if (fi.arrayDims > 0 && !fi.arraySizes.empty()) {
                llvm::Type* arrTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
                for (auto it2 = fi.arraySizes.rbegin(); it2 != fi.arraySizes.rend(); ++it2)
                    arrTy = llvm::ArrayType::get(arrTy, *it2);
                currentTy = arrTy;
            } else {
                currentTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
            }
        }
    }

    // Now currentPtr points to the array field; index into it
    auto expressions  = ctx->expression();
    size_t numIndices = ctx->LBRACKET().size();
    // expressions layout: [idx0, idx1, ..., idxN-1, rhs]

    // Handle Extended types (Vec, Map, Set) for field index assignment
    // e.g., struct.field[idx] = val where field is Vec/Map/Set
    if (currentTI && currentTI->kind == TypeKind::Extended) {
        emitExtendedIndexAssign(currentPtr, currentTI, expressions);
        return {};
    }

    // The field must be an array type (possibly behind a pointer)
    if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
        // Pointer-to-array or pointer: load the pointer then GEP by index
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        currentPtr  = builder_->CreateLoad(ptrTy, currentPtr, "field_ptr");
        currentTI   = currentTI->pointeeType;
        currentTy   = currentTI->toLLVMType(*context_, module_->getDataLayout());

        for (size_t i = 0; i < numIndices; i++) {
            auto* idx = castValue(visit(expressions[i]));
            if (idx->getType() != i64Ty)
                idx = builder_->CreateIntCast(idx, i64Ty, true);

            if (auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(currentTy)) {
                currentPtr = builder_->CreateInBoundsGEP(
                    arrTy, currentPtr,
                    { llvm::ConstantInt::get(i64Ty, 0), idx }, "elem_ptr");
                currentTy = arrTy->getElementType();
            } else {
                currentPtr = builder_->CreateInBoundsGEP(
                    currentTy, currentPtr, idx, "elem_ptr");
            }
        }

        auto* val0 = castValue(visit(expressions[expressions.size() - 1]));
        if (val0->getType() != currentTy) {
            if (val0->getType()->isIntegerTy() && currentTy->isIntegerTy())
                val0 = builder_->CreateIntCast(val0, currentTy, true);
            else if (val0->getType()->isFloatingPointTy() && currentTy->isFloatingPointTy()) {
                if (val0->getType()->getPrimitiveSizeInBits() > currentTy->getPrimitiveSizeInBits())
                    val0 = builder_->CreateFPTrunc(val0, currentTy);
                else
                    val0 = builder_->CreateFPExt(val0, currentTy);
            }
        }
        builder_->CreateStore(val0, currentPtr);
        return {};
    } else {
        // Direct array field
        std::vector<llvm::Value*> gepIndices;
        gepIndices.push_back(llvm::ConstantInt::get(i64Ty, 0));
        for (size_t i = 0; i < numIndices; i++) {
            auto* idx = castValue(visit(expressions[i]));
            if (idx->getType() != i64Ty)
                idx = builder_->CreateIntCast(idx, i64Ty, true);
            gepIndices.push_back(idx);
        }

        llvm::Type* elemTy = currentTy;
        for (size_t i = 0; i < numIndices; i++) {
            auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(elemTy);
            if (!arrTy) {
                std::cerr << "lucis: too many indices for field array\n";
                return {};
            }
            elemTy = arrTy->getElementType();
        }

        currentPtr = builder_->CreateGEP(currentTy, currentPtr, gepIndices, "elem_ptr");
        currentTy  = elemTy;

        // RHS is the last expression
        auto* val1 = castValue(visit(expressions[expressions.size() - 1]));
        if (val1->getType() != currentTy) {
            if (val1->getType()->isIntegerTy() && currentTy->isIntegerTy())
                val1 = builder_->CreateIntCast(val1, currentTy, true);
            else if (val1->getType()->isFloatingPointTy() && currentTy->isFloatingPointTy()) {
                if (val1->getType()->getPrimitiveSizeInBits() > currentTy->getPrimitiveSizeInBits())
                    val1 = builder_->CreateFPTrunc(val1, currentTy);
                else
                    val1 = builder_->CreateFPExt(val1, currentTy);
            }
        }
        builder_->CreateStore(val1, currentPtr);
        return {};
    }
}

// ptr->field = value;


std::any IRGen::visitArrowAssignStmt(LucisParser::ArrowAssignStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto identifiers = ctx->IDENTIFIER();
    if (identifiers.size() < 2) {
        std::cerr << "lucis: malformed '->' assignment\n";
        return {};
    }

    auto varName = identifiers[0]->getText();
    auto fieldName = identifiers.back()->getText();

    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        std::cerr << "lucis: undefined variable '" << varName << "'\n";
        return {};
    }

    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
    const TypeInfo* currentTI = it->second.typeInfo;
    llvm::Value* basePtr = nullptr;
    bool pointerBase = false;

    if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
        basePtr = builder_->CreateLoad(ptrTy, it->second.alloca, varName + "_ptr");
        currentTI = currentTI->pointeeType;
        pointerBase = true;
    } else {
        basePtr = it->second.alloca;
    }

    // Resolve dotted base before arrow: a.b.c->x
    for (size_t i = 1; i + 1 < identifiers.size(); i++) {
        auto baseField = identifiers[i]->getText();
        if (!currentTI || (currentTI->kind != TypeKind::Struct && currentTI->kind != TypeKind::Union)) {
            std::cerr << "lucis: cannot access field '" << baseField
                      << "' on non-struct type\n";
            return {};
        }

        int idx = -1;
        const TypeInfo* nextTI = nullptr;
        for (size_t f = 0; f < currentTI->fields.size(); f++) {
            if (currentTI->fields[f].name == baseField) {
                idx = static_cast<int>(f);
                nextTI = currentTI->fields[f].typeInfo;
                break;
            }
        }
        if (idx < 0 || !nextTI) {
            std::cerr << "lucis: '" << currentTI->name
                      << "' has no field '" << baseField << "'\n";
            return {};
        }

        if (currentTI->kind == TypeKind::Union) {
            currentTI = nextTI;
        } else {
            auto* currentLLTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
            auto* gep = builder_->CreateStructGEP(currentLLTy, basePtr, idx, baseField + "_ptr");
            basePtr = gep;
            currentTI = nextTI;
        }

        if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
            basePtr = builder_->CreateLoad(ptrTy, basePtr, baseField + "_load");
            currentTI = currentTI->pointeeType;
            pointerBase = true;
        }
    }

    if (!pointerBase || !currentTI ||
        (currentTI->kind != TypeKind::Struct && currentTI->kind != TypeKind::Union)) {
        std::cerr << "lucis: '->' requires pointer to struct or union\n";
        return {};
    }

    int fieldIdx = -1;
    const TypeInfo* fieldTI = nullptr;
    for (size_t f = 0; f < currentTI->fields.size(); f++) {
        if (currentTI->fields[f].name == fieldName) {
            fieldIdx = static_cast<int>(f);
            fieldTI = currentTI->fields[f].typeInfo;
            break;
        }
    }
    if (fieldIdx < 0 || !fieldTI) {
        std::cerr << "lucis: '" << currentTI->name
                  << "' has no field '" << fieldName << "'\n";
        return {};
    }

    auto* val = castValue(visit(ctx->expression()));
    auto* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());

    if (val->getType() != fieldLLTy) {
        if (val->getType()->isIntegerTy() && fieldLLTy->isIntegerTy())
            val = builder_->CreateIntCast(val, fieldLLTy, fieldTI->isSigned);
        else if (val->getType()->isFloatingPointTy() && fieldLLTy->isFloatingPointTy())
            val = builder_->CreateFPCast(val, fieldLLTy);
    }

    if (currentTI->kind == TypeKind::Union) {
        // Union: store at offset 0 (same as base pointer)
        builder_->CreateStore(val, basePtr);
    } else {
        auto* structLLTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
        auto* gep = builder_->CreateStructGEP(structLLTy, basePtr, fieldIdx, fieldName + "_ptr");
        builder_->CreateStore(val, gep);
    }
    return {};
}

// ptr->field += value;


std::any IRGen::visitArrowCompoundAssignStmt(
    LucisParser::ArrowCompoundAssignStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto identifiers = ctx->IDENTIFIER();
    if (identifiers.size() < 2) {
        std::cerr << "lucis: malformed '->' compound assignment\n";
        return {};
    }

    auto varName = identifiers[0]->getText();
    auto fieldName = identifiers.back()->getText();

    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        std::cerr << "lucis: undefined variable '" << varName << "'\n";
        return {};
    }

    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
    const TypeInfo* currentTI = it->second.typeInfo;
    llvm::Value* basePtr = nullptr;
    bool pointerBase = false;

    if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
        basePtr = builder_->CreateLoad(ptrTy, it->second.alloca, varName + "_ptr");
        currentTI = currentTI->pointeeType;
        pointerBase = true;
    } else {
        basePtr = it->second.alloca;
    }

    for (size_t i = 1; i + 1 < identifiers.size(); i++) {
        auto baseField = identifiers[i]->getText();
        if (!currentTI || (currentTI->kind != TypeKind::Struct && currentTI->kind != TypeKind::Union)) {
            std::cerr << "lucis: cannot access field '" << baseField
                      << "' on non-struct type\n";
            return {};
        }

        int idx = -1;
        const TypeInfo* nextTI = nullptr;
        for (size_t f = 0; f < currentTI->fields.size(); f++) {
            if (currentTI->fields[f].name == baseField) {
                idx = static_cast<int>(f);
                nextTI = currentTI->fields[f].typeInfo;
                break;
            }
        }
        if (idx < 0 || !nextTI) {
            std::cerr << "lucis: '" << currentTI->name
                      << "' has no field '" << baseField << "'\n";
            return {};
        }

        if (currentTI->kind == TypeKind::Union) {
            currentTI = nextTI;
        } else {
            auto* currentLLTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
            auto* gepField = builder_->CreateStructGEP(currentLLTy, basePtr, idx, baseField + "_ptr");
            basePtr = gepField;
            currentTI = nextTI;
        }

        if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
            basePtr = builder_->CreateLoad(ptrTy, basePtr, baseField + "_load");
            currentTI = currentTI->pointeeType;
            pointerBase = true;
        }
    }

    if (!pointerBase || !currentTI || currentTI->kind != TypeKind::Struct) {
        std::cerr << "lucis: '->' requires pointer to struct\n";
        return {};
    }

    int fieldIdx = -1;
    for (size_t f = 0; f < currentTI->fields.size(); f++) {
        if (currentTI->fields[f].name == fieldName) {
            fieldIdx = static_cast<int>(f);
            break;
        }
    }
        if (fieldIdx < 0) {
            std::cerr << "lucis: [FA1] '" << currentTI->name
                      << "' has no field '" << fieldName << "'\n";
            return {};
        }

    auto* structLLTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
    auto* gep = builder_->CreateStructGEP(structLLTy, basePtr, fieldIdx, fieldName + "_ptr");
    auto* fieldTy = currentTI->fields[fieldIdx].typeInfo
                        ->toLLVMType(*context_, module_->getDataLayout());

    auto* fieldTI = currentTI->fields[fieldIdx].typeInfo;
    auto* oldVal = builder_->CreateLoad(fieldTy, gep, fieldName + "_old");
    if (fieldTI->isVolatile)
        llvm::cast<llvm::LoadInst>(oldVal)->setVolatile(true);
    auto* rhs = castValue(visit(ctx->expression()));

    if (rhs->getType() != fieldTy) {
        if (rhs->getType()->isIntegerTy() && fieldTy->isIntegerTy())
            rhs = builder_->CreateIntCast(rhs, fieldTy, fieldTI->isSigned);
    }

    auto acStore = [&](llvm::Value* v, llvm::Value* p) {
        auto* s = builder_->CreateStore(v, p);
        if (fieldTI->isVolatile) s->setVolatile(true);
    };

    // ── Null-coalescing: ptr->field ??= default ──────────────────
    if (ctx->op->getType() == LucisLexer::NULLCOAL_ASSIGN) {
        auto* isNull = builder_->CreateICmpEQ(
            oldVal, llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_)),
            "ncoal_isnull");
        auto* thenBB = llvm::BasicBlock::Create(*context_, "ncoal.store", currentFunction_);
        auto* mergeBB = llvm::BasicBlock::Create(*context_, "ncoal.end", currentFunction_);
        builder_->CreateCondBr(isNull, thenBB, mergeBB);
        builder_->SetInsertPoint(thenBB);
        acStore(rhs, gep);
        builder_->CreateBr(mergeBB);
        builder_->SetInsertPoint(mergeBB);
        return {};
    }

    llvm::Value* result;
    auto opType = ctx->op->getType();
    bool isFloat = fieldTy->isFloatingPointTy();

    // Pointer arithmetic on pointer field accessed via ->: field += int
    if (fieldTI && fieldTI->kind == TypeKind::Pointer &&
        (opType == LucisLexer::PLUS_ASSIGN || opType == LucisLexer::MINUS_ASSIGN)) {
        auto* pointeeLLTy = fieldTI->pointeeType
            ? fieldTI->pointeeType->toLLVMType(*context_, module_->getDataLayout())
            : llvm::Type::getInt8Ty(*context_);
        auto* idx = rhs;
        if (opType == LucisLexer::MINUS_ASSIGN)
            idx = builder_->CreateNeg(rhs, "neg");
        result = static_cast<llvm::Value*>(
            builder_->CreateGEP(pointeeLLTy, oldVal, idx, "ptr_cmpd"));
        acStore(result, gep);
        return {};
    }

    // Division by zero guard for /= and %=
    if (!isFloat && (opType == LucisLexer::SLASH_ASSIGN ||
                     opType == LucisLexer::PERCENT_ASSIGN)) {
        emitDivByZeroGuard(rhs, ctx->op);
    }

    if (opType == LucisLexer::PLUS_ASSIGN)
        result = isFloat ? builder_->CreateFAdd(oldVal, rhs) : builder_->CreateAdd(oldVal, rhs);
    else if (opType == LucisLexer::MINUS_ASSIGN)
        result = isFloat ? builder_->CreateFSub(oldVal, rhs) : builder_->CreateSub(oldVal, rhs);
    else if (opType == LucisLexer::STAR_ASSIGN)
        result = isFloat ? builder_->CreateFMul(oldVal, rhs) : builder_->CreateMul(oldVal, rhs);
    else if (opType == LucisLexer::SLASH_ASSIGN)
        result = isFloat ? builder_->CreateFDiv(oldVal, rhs)
                         : (currentTI->fields[fieldIdx].typeInfo->isSigned
                                ? builder_->CreateSDiv(oldVal, rhs)
                                : builder_->CreateUDiv(oldVal, rhs));
    else if (opType == LucisLexer::PERCENT_ASSIGN)
        result = isFloat ? builder_->CreateFRem(oldVal, rhs)
                         : (currentTI->fields[fieldIdx].typeInfo->isSigned
                                ? builder_->CreateSRem(oldVal, rhs)
                                : builder_->CreateURem(oldVal, rhs));
    else if (opType == LucisLexer::AMP_ASSIGN)
        result = builder_->CreateAnd(oldVal, rhs);
    else if (opType == LucisLexer::PIPE_ASSIGN)
        result = builder_->CreateOr(oldVal, rhs);
    else if (opType == LucisLexer::CARET_ASSIGN)
        result = builder_->CreateXor(oldVal, rhs);
    else if (opType == LucisLexer::LSHIFT_ASSIGN)
        result = builder_->CreateShl(oldVal, rhs);
    else if (opType == LucisLexer::RSHIFT_ASSIGN)
        result = currentTI->fields[fieldIdx].typeInfo->isSigned
                     ? builder_->CreateAShr(oldVal, rhs)
                     : builder_->CreateLShr(oldVal, rhs);
    else {
        std::cerr << "lucis: unsupported compound assign operator\n";
        return {};
    }

    acStore(result, gep);
    return {};
}

// Shared helper: resolve an arrow-based lvalue chain to a store pointer.


IRGen::ArrowLValue IRGen::resolveArrowLValue(
    const std::vector<antlr4::tree::TerminalNode*>& identifiers,
    const std::vector<antlr4::tree::TerminalNode*>& dotIdentifiers,
    size_t numArrows,
    size_t numBrackets,
    const std::vector<LucisParser::ExpressionContext*>& indexExprs)
{
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);
    auto* ptrTy = llvm::PointerType::getUnqual(*context_);

    if (identifiers.size() < 1 + numArrows) {
        std::cerr << "lucis: malformed arrow lvalue\n";
        return {};
    }
    auto varName = identifiers[0]->getText();
    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        std::cerr << "lucis: undefined variable '" << varName << "'\n";
        return {};
    }

    llvm::Value* basePtr = it->second.alloca;
    const TypeInfo* currentTI = it->second.typeInfo;

    if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
        basePtr = builder_->CreateLoad(ptrTy, basePtr, varName + "_ptr");
        currentTI = currentTI->pointeeType;
    }

    for (size_t i = 0; i < dotIdentifiers.size(); i++) {
        auto fieldName = identifiers[1 + i]->getText();
        if (!currentTI || (currentTI->kind != TypeKind::Struct &&
                           currentTI->kind != TypeKind::Union)) {
            std::cerr << "lucis: cannot access field '" << fieldName
                      << "' on non-struct type\n";
            return {};
        }
        int idx = -1;
        for (size_t f = 0; f < currentTI->fields.size(); f++) {
            if (currentTI->fields[f].name == fieldName) {
                idx = static_cast<int>(f);
                break;
            }
        }
        if (idx < 0) {
            std::cerr << "lucis: '" << currentTI->name
                      << "' has no field '" << fieldName << "'\n";
            return {};
        }
        if (currentTI->kind == TypeKind::Union) {
            currentTI = currentTI->fields[idx].typeInfo;
        } else {
            auto* curTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
            basePtr = builder_->CreateStructGEP(curTy, basePtr, idx, fieldName + "_ptr");
            currentTI = currentTI->fields[idx].typeInfo;
        }
        if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
            basePtr = builder_->CreateLoad(ptrTy, basePtr, fieldName + "_load");
            currentTI = currentTI->pointeeType;
        }
    }

    size_t firstArrowIdx = 1 + dotIdentifiers.size();
    size_t lastArrowIdx = firstArrowIdx + numArrows - 1;

    for (size_t i = firstArrowIdx; i < lastArrowIdx; i++) {
        auto fieldName = identifiers[i]->getText();
        if (!currentTI || (currentTI->kind != TypeKind::Struct &&
                           currentTI->kind != TypeKind::Union)) {
            std::cerr << "lucis: '->' requires pointer to struct, got '"
                      << (currentTI ? currentTI->name : "?") << "'\n";
            return {};
        }
        int idx = -1;
        for (size_t f = 0; f < currentTI->fields.size(); f++) {
            if (currentTI->fields[f].name == fieldName) {
                idx = static_cast<int>(f);
                break;
            }
        }
        if (idx < 0) {
            std::cerr << "lucis: '" << currentTI->name
                      << "' has no field '" << fieldName << "'\n";
            return {};
        }
        auto* curTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
        auto* gep = builder_->CreateStructGEP(curTy, basePtr, idx, fieldName + "_ptr");
        currentTI = currentTI->fields[idx].typeInfo;
        if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
            basePtr = builder_->CreateLoad(ptrTy, gep, fieldName + "_load");
            currentTI = currentTI->pointeeType;
        } else {
            basePtr = gep;
        }
    }

    if (numArrows == 0) {
        std::cerr << "lucis: arrow lvalue requires at least one arrow\n";
        return {};
    }

    auto fieldName = identifiers[lastArrowIdx]->getText();
    if (!currentTI || (currentTI->kind != TypeKind::Struct &&
                       currentTI->kind != TypeKind::Union)) {
        std::cerr << "lucis: '->' requires pointer to struct, got '"
                  << (currentTI ? currentTI->name : "?") << "'\n";
        return {};
    }
    int fieldIdx = -1;
    const TypeInfo* fieldTI = nullptr;
    for (size_t f = 0; f < currentTI->fields.size(); f++) {
        if (currentTI->fields[f].name == fieldName) {
            fieldIdx = static_cast<int>(f);
            fieldTI = currentTI->fields[f].typeInfo;
            break;
        }
    }
    if (fieldIdx < 0) {
        std::cerr << "lucis: '" << currentTI->name
                  << "' has no field '" << fieldName << "'\n";
        return {};
    }

    const auto& fi = currentTI->fields[fieldIdx];
    llvm::Type* fieldTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
    if (fi.arrayDims > 0 && !fi.arraySizes.empty()) {
        llvm::Type* arrTy = fieldTy;
        for (auto it = fi.arraySizes.rbegin(); it != fi.arraySizes.rend(); ++it)
            arrTy = llvm::ArrayType::get(arrTy, *it);
        fieldTy = arrTy;
    }

    llvm::Value* destPtr;
    if (currentTI->kind == TypeKind::Union) {
        destPtr = basePtr;
    } else {
        auto* curTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
        destPtr = builder_->CreateStructGEP(curTy, basePtr, fieldIdx, fieldName + "_ptr");
    }

    // Extended types (Map/Vec/Set) are handled by the caller — skip
    if (fieldTI && fieldTI->kind == TypeKind::Extended)
        return {};

    // Pointer field: load pointer and index via pointer arithmetic
    if (fieldTI && fieldTI->kind == TypeKind::Pointer) {
        destPtr = builder_->CreateLoad(llvm::PointerType::getUnqual(*context_), destPtr, fieldName + "_ptr");
        fieldTy = fieldTI->pointeeType->toLLVMType(*context_, module_->getDataLayout());
        fieldTI = fieldTI->pointeeType;
    }

    for (size_t ai = 0; ai < numBrackets; ai++) {
        auto* idxVal = castValue(visit(indexExprs[ai]));
        if (idxVal->getType() != i64Ty)
            idxVal = builder_->CreateIntCast(idxVal, i64Ty, true);
        if (fieldTy && fieldTy->isArrayTy()) {
            destPtr = builder_->CreateInBoundsGEP(fieldTy, destPtr,
                { llvm::ConstantInt::get(i64Ty, 0), idxVal },
                fieldName + "_elem");
            fieldTy = llvm::cast<llvm::ArrayType>(fieldTy)->getElementType();
            if (fieldTI && fieldTI->elementType)
                fieldTI = fieldTI->elementType;
        } else if (fieldTI) {
            // Pointer or scalar: simple GEP
            destPtr = builder_->CreateGEP(fieldTy, destPtr, idxVal, fieldName + "_elem");
            if (fieldTI->pointeeType) {
                fieldTy = fieldTI->pointeeType->toLLVMType(*context_, module_->getDataLayout());
                fieldTI = fieldTI->pointeeType;
            }
        }
    }

    return { destPtr, fieldTy, fieldTI };
}

// ptr->field[i] = expr;  or  ptr->field1->field2[i] = expr;
// General resolver for mixed dot/arrow/index lvalue chains.
// identifiers = [var, field1, field2, ..., fieldN]
// dots and arrows in source order — the function sorts them by token position
// to correctly interleave dot and arrow accesses.


IRGen::ArrowLValue IRGen::resolveArrowAnyLValue(
    const std::vector<antlr4::tree::TerminalNode*>& identifiers,
    const std::vector<antlr4::tree::TerminalNode*>& dots,
    const std::vector<antlr4::tree::TerminalNode*>& arrows,
    const std::vector<LucisParser::ExpressionContext*>& indexExprs)
{
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);
    auto* ptrTy = llvm::PointerType::getUnqual(*context_);

    if (identifiers.empty()) {
        std::cerr << "lucis: malformed arrow lvalue\n";
        return {};
    }
    auto varName = identifiers[0]->getText();
    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        std::cerr << "lucis: undefined variable '" << varName << "'\n";
        return {};
    }

    llvm::Value* basePtr = it->second.alloca;
    const TypeInfo* currentTI = it->second.typeInfo;

    if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
        basePtr = builder_->CreateLoad(ptrTy, basePtr, varName + "_ptr");
        currentTI = currentTI->pointeeType;
    }

    // Build merged list of (tokenIndex, isArrow) for all separators
    std::vector<std::pair<size_t, bool>> ops;
    for (auto* d : dots)
        ops.emplace_back(d->getSymbol()->getTokenIndex(), false);
    for (auto* a : arrows)
        ops.emplace_back(a->getSymbol()->getTokenIndex(), true);
    std::sort(ops.begin(), ops.end());

    size_t numFieldAccesses = ops.size(); // each op has an identifier after it
    if (identifiers.size() != 1 + numFieldAccesses) {
        std::cerr << "lucis: mismatch between identifiers and operators in arrow lvalue\n";
        return {};
    }

    // Process all field accesses except the last one
    for (size_t i = 0; i + 1 < numFieldAccesses; i++) {
        auto fieldName = identifiers[1 + i]->getText();
        if (!currentTI || (currentTI->kind != TypeKind::Struct &&
                           currentTI->kind != TypeKind::Union)) {
            std::cerr << "lucis: cannot access field '" << fieldName
                      << "' on non-struct type\n";
            return {};
        }
        int idx = -1;
        for (size_t f = 0; f < currentTI->fields.size(); f++) {
            if (currentTI->fields[f].name == fieldName) {
                idx = static_cast<int>(f);
                break;
            }
        }
        if (idx < 0) {
            std::cerr << "lucis: '" << currentTI->name
                      << "' has no field '" << fieldName << "'\n";
            return {};
        }

        if (ops[i].second) {
            // Arrow: deref pointer first, then GEP
            if (!currentTI || (currentTI->kind != TypeKind::Struct &&
                               currentTI->kind != TypeKind::Union)) {
                std::cerr << "lucis: '->' requires struct type, got '"
                          << (currentTI ? currentTI->name : "?") << "'\n";
                return {};
            }
            auto* curTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
            auto* gep = builder_->CreateStructGEP(curTy, basePtr, idx, fieldName + "_ptr");
            currentTI = currentTI->fields[idx].typeInfo;
            if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
                basePtr = builder_->CreateLoad(ptrTy, gep, fieldName + "_load");
                currentTI = currentTI->pointeeType;
            } else {
                basePtr = gep;
            }
        } else {
            // Dot: struct GEP with auto-deref for pointer fields
            if (currentTI->kind == TypeKind::Union) {
                currentTI = currentTI->fields[idx].typeInfo;
            } else {
                auto* curTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
                basePtr = builder_->CreateStructGEP(curTy, basePtr, idx, fieldName + "_ptr");
                currentTI = currentTI->fields[idx].typeInfo;
            }
            if (currentTI && currentTI->kind == TypeKind::Pointer && currentTI->pointeeType) {
                basePtr = builder_->CreateLoad(ptrTy, basePtr, fieldName + "_load");
                currentTI = currentTI->pointeeType;
            }
        }
    }

    // Last field access
    auto fieldName = identifiers.back()->getText();
    if (!currentTI || (currentTI->kind != TypeKind::Struct &&
                       currentTI->kind != TypeKind::Union)) {
        std::cerr << "lucis: cannot access final field '" << fieldName
                  << "' on non-struct type\n";
        return {};
    }
    int fieldIdx = -1;
    const TypeInfo* fieldTI = nullptr;
    for (size_t f = 0; f < currentTI->fields.size(); f++) {
        if (currentTI->fields[f].name == fieldName) {
            fieldIdx = static_cast<int>(f);
            fieldTI = currentTI->fields[f].typeInfo;
            break;
        }
    }
    if (fieldIdx < 0) {
        std::cerr << "lucis: '" << currentTI->name
                  << "' has no field '" << fieldName << "'\n";
        return {};
    }

    const auto& fi = currentTI->fields[fieldIdx];
    llvm::Type* fieldTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
    if (fi.arrayDims > 0 && !fi.arraySizes.empty()) {
        llvm::Type* arrTy = fieldTy;
        for (auto it = fi.arraySizes.rbegin(); it != fi.arraySizes.rend(); ++it)
            arrTy = llvm::ArrayType::get(arrTy, *it);
        fieldTy = arrTy;
    }

    llvm::Value* destPtr;
    if (currentTI->kind == TypeKind::Union) {
        destPtr = basePtr;
    } else {
        auto* curTy = currentTI->toLLVMType(*context_, module_->getDataLayout());
        destPtr = builder_->CreateStructGEP(curTy, basePtr, fieldIdx, fieldName + "_ptr");
    }

    // Extended types (Map/Vec/Set) are handled by the caller — expose the
    // container header pointer so callers can emit the subscript assignment
    if (fieldTI && fieldTI->kind == TypeKind::Extended) {
        ArrowLValue lv;
        lv.ptr = destPtr;
        lv.ty = fieldTy;
        lv.fieldTI = fieldTI;
        return lv;
    }

    // Pointer field: load pointer and index via pointer arithmetic
    if (fieldTI && fieldTI->kind == TypeKind::Pointer && fieldTI->pointeeType) {
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        destPtr = builder_->CreateLoad(ptrTy, destPtr, fieldName + "_ptr");
        fieldTy = fieldTI->pointeeType->toLLVMType(*context_, module_->getDataLayout());
        fieldTI = fieldTI->pointeeType;
    }

    // Array indices
    for (size_t ai = 0; ai < indexExprs.size(); ai++) {
        auto* idxVal = castValue(visit(indexExprs[ai]));
        if (idxVal->getType() != i64Ty)
            idxVal = builder_->CreateIntCast(idxVal, i64Ty, true);
        if (fieldTy && fieldTy->isArrayTy()) {
            destPtr = builder_->CreateInBoundsGEP(fieldTy, destPtr,
                { llvm::ConstantInt::get(i64Ty, 0), idxVal },
                fieldName + "_idx" + std::to_string(ai));
            fieldTy = fieldTy->getArrayElementType();
        } else if (fieldTI) {
            destPtr = builder_->CreateGEP(fieldTy, destPtr, idxVal, fieldName + "_idx");
            if (fieldTI->pointeeType) {
                fieldTy = fieldTI->pointeeType->toLLVMType(*context_, module_->getDataLayout());
                fieldTI = fieldTI->pointeeType;
            }
        }
    }
    if (fieldTI && !fieldTy->isArrayTy() && fieldTI->elementType)
        fieldTI = fieldTI->elementType;

    return { destPtr, fieldTy, fieldTI };
}

// General arrow lvalue assignment (dot/arrow/index chains)


std::any IRGen::visitArrowAnyAssignStmt(LucisParser::ArrowAnyAssignStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto identifiers = ctx->IDENTIFIER();
    auto dots = ctx->DOT();
    auto arrows = ctx->ARROW();
    auto numBrackets = ctx->LBRACKET().size();
    auto expressions = ctx->expression();

    if (identifiers.size() < 2) {
        std::cerr << "lucis: arrow assign needs at least one arrow\n";
        return {};
    }

    size_t numIndexExprs = numBrackets;
    std::vector<LucisParser::ExpressionContext*> indexExprs;
    for (size_t i = 0; i < numIndexExprs; i++)
        indexExprs.push_back(expressions[i]);
    auto* rhsExpr = expressions[numIndexExprs];

    auto lv = resolveArrowAnyLValue(identifiers, dots, arrows, indexExprs);

    // Extended container (Vec/Map/Set) subscript assignment: ptr->vec[i] = v
    if (lv.fieldTI && lv.fieldTI->kind == TypeKind::Extended) {
        emitExtendedIndexAssign(lv.ptr, lv.fieldTI, expressions);
        return {};
    }
    if (!lv.ptr) return {};

    auto* val = castValue(visit(rhsExpr));
    if (val->getType() != lv.ty) {
        if (val->getType()->isIntegerTy() && lv.ty->isIntegerTy())
            val = builder_->CreateIntCast(val, lv.ty, lv.fieldTI && lv.fieldTI->isSigned);
        else if (val->getType()->isFloatingPointTy() && lv.ty->isFloatingPointTy())
            val = builder_->CreateFPCast(val, lv.ty);
    }
    builder_->CreateStore(val, lv.ptr);
    return {};
}

// General arrow lvalue compound assignment


std::any IRGen::visitArrowAnyCompoundAssignStmt(
    LucisParser::ArrowAnyCompoundAssignStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto identifiers = ctx->IDENTIFIER();
    auto dots = ctx->DOT();
    auto arrows = ctx->ARROW();
    auto numBrackets = ctx->LBRACKET().size();
    auto expressions = ctx->expression();

    if (identifiers.size() < 2) return {};

    size_t numIndexExprs = numBrackets;
    std::vector<LucisParser::ExpressionContext*> indexExprs;
    for (size_t i = 0; i < numIndexExprs; i++)
        indexExprs.push_back(expressions[i]);
    auto* rhsExpr = expressions[numIndexExprs];

    auto lv = resolveArrowAnyLValue(identifiers, dots, arrows, indexExprs);
    if (!lv.ptr) return {};

    auto* oldVal = builder_->CreateLoad(lv.ty, lv.ptr, "arrow_any_old");
    if (lv.fieldTI && lv.fieldTI->isVolatile)
        llvm::cast<llvm::LoadInst>(oldVal)->setVolatile(true);
    auto* rhs = castValue(visit(rhsExpr));
    if (rhs->getType() != lv.ty) {
        if (rhs->getType()->isIntegerTy() && lv.ty->isIntegerTy())
            rhs = builder_->CreateIntCast(rhs, lv.ty, lv.fieldTI && lv.fieldTI->isSigned);
    }

    auto avStore = [&](llvm::Value* v, llvm::Value* p) {
        auto* s = builder_->CreateStore(v, p);
        if (lv.fieldTI && lv.fieldTI->isVolatile) s->setVolatile(true);
    };

    // ── Null-coalescing: arrow_any ??= default ──────────────────
    if (ctx->op->getType() == LucisLexer::NULLCOAL_ASSIGN) {
        auto* isNull = builder_->CreateICmpEQ(
            oldVal, llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_)),
            "ncoal_isnull");
        auto* thenBB = llvm::BasicBlock::Create(*context_, "ncoal.store", currentFunction_);
        auto* mergeBB = llvm::BasicBlock::Create(*context_, "ncoal.end", currentFunction_);
        builder_->CreateCondBr(isNull, thenBB, mergeBB);
        builder_->SetInsertPoint(thenBB);
        avStore(rhs, lv.ptr);
        builder_->CreateBr(mergeBB);
        builder_->SetInsertPoint(mergeBB);
        return {};
    }

    bool isFloat = lv.ty->isFloatingPointTy();
    auto opType = ctx->op->getType();
    llvm::Value* result;

    // Pointer arithmetic on pointer field: field += int / field -= int
    if (lv.fieldTI && lv.fieldTI->kind == TypeKind::Pointer &&
        (opType == LucisLexer::PLUS_ASSIGN || opType == LucisLexer::MINUS_ASSIGN)) {
        auto* pointeeLLTy = lv.fieldTI->pointeeType
            ? lv.fieldTI->pointeeType->toLLVMType(*context_, module_->getDataLayout())
            : llvm::Type::getInt8Ty(*context_);
        auto* idx = rhs;
        if (opType == LucisLexer::MINUS_ASSIGN)
            idx = builder_->CreateNeg(rhs, "neg");
        result = static_cast<llvm::Value*>(
            builder_->CreateGEP(pointeeLLTy, oldVal, idx, "ptr_cmpd"));
        avStore(result, lv.ptr);
        return {};
    }

    if (opType == LucisLexer::PLUS_ASSIGN)
        result = isFloat ? builder_->CreateFAdd(oldVal, rhs) : builder_->CreateAdd(oldVal, rhs);
    else if (opType == LucisLexer::MINUS_ASSIGN)
        result = isFloat ? builder_->CreateFSub(oldVal, rhs) : builder_->CreateSub(oldVal, rhs);
    else if (opType == LucisLexer::STAR_ASSIGN)
        result = isFloat ? builder_->CreateFMul(oldVal, rhs) : builder_->CreateMul(oldVal, rhs);
    else if (opType == LucisLexer::SLASH_ASSIGN)
        result = isFloat ? builder_->CreateFDiv(oldVal, rhs)
                         : (lv.fieldTI && lv.fieldTI->isSigned
                                ? builder_->CreateSDiv(oldVal, rhs)
                                : builder_->CreateUDiv(oldVal, rhs));
    else if (opType == LucisLexer::PERCENT_ASSIGN)
        result = isFloat ? builder_->CreateFRem(oldVal, rhs)
                         : (lv.fieldTI && lv.fieldTI->isSigned
                                ? builder_->CreateSRem(oldVal, rhs)
                                : builder_->CreateURem(oldVal, rhs));
    else if (opType == LucisLexer::AMP_ASSIGN)
        result = builder_->CreateAnd(oldVal, rhs);
    else if (opType == LucisLexer::PIPE_ASSIGN)
        result = builder_->CreateOr(oldVal, rhs);
    else if (opType == LucisLexer::CARET_ASSIGN)
        result = builder_->CreateXor(oldVal, rhs);
    else if (opType == LucisLexer::LSHIFT_ASSIGN)
        result = builder_->CreateShl(oldVal, rhs);
    else if (opType == LucisLexer::RSHIFT_ASSIGN)
        result = (lv.fieldTI && lv.fieldTI->isSigned)
                     ? builder_->CreateAShr(oldVal, rhs)
                     : builder_->CreateLShr(oldVal, rhs);
    else {
        std::cerr << "lucis: unsupported arrow compound assign\n";
        return {};
    }
    avStore(result, lv.ptr);
    return {};
}



std::any IRGen::visitExprStmt(LucisParser::ExprStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto exprAny = visit(ctx->expression());
    auto* expr = ctx->expression();
    auto* exprTI = resolveExprTypeInfo(expr);
    auto exprDims = resolveExprArrayDims(expr);
    if (!exprTI || exprDims > 0 || exprTI->kind != TypeKind::String)
        return {};
    if (dynamic_cast<LucisParser::IdentExprContext*>(expr))
        return {};
    if (isBorrowedStringExpr(expr))
        return {};

    auto* value = castValue(exprAny);
    auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
    auto* strPtr = builder_->CreateExtractValue(value, 0, "tmp_str_ptr");
    auto* strLen = builder_->CreateExtractValue(value, 1, "tmp_str_len");
    auto callee = declareBuiltin("lucis_freeStr", llvm::Type::getVoidTy(*context_),
                                 {ptrTy, usizeTy});
    builder_->CreateCall(callee, {strPtr, strLen});
    return {};
}

// println(x);


std::any IRGen::visitCallStmt(LucisParser::CallStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto funcName = ctx->IDENTIFIER()->getText();

    // ── Comptime function call as statement ───────────────────────
    if (comptimeEngine_ && comptimeEngine_->isReady() &&
        comptimeEngine_->registry().isComptime(funcName)) {
        std::vector<llvm::Value*> argVals;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                argVals.push_back(castValue(visit(exprCtx)));
        }
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
                return {};
            }
        }
        auto* decl = comptimeEngine_->registry().lookup(funcName);
        comptimeEngine_->evaluate(decl, cvArgs);
        return {};
    }

    auto cleanupTempArg = [&](LucisParser::ExpressionContext* argExpr, llvm::Value* argVal) {
        if (!argExpr || !argVal) return;
        if (dynamic_cast<LucisParser::IdentExprContext*>(argExpr)) return;

        auto* argTI = resolveExprTypeInfo(argExpr);
        if (!argTI) return;
        if (resolveExprArrayDims(argExpr) > 0) return;

        auto* voidTy = llvm::Type::getVoidTy(*context_);
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);

        if (argTI->kind == TypeKind::String) {
            if (isBorrowedStringExpr(argExpr)) return;
            auto* strPtr = builder_->CreateExtractValue(argVal, 0, "tmp_arg_ptr");
            auto* strLen = builder_->CreateExtractValue(argVal, 1, "tmp_arg_len");
            auto callee = declareBuiltin("lucis_freeStr", voidTy, {ptrTy, usizeTy});
            builder_->CreateCall(callee, {strPtr, strLen});
            return;
        }

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

    std::vector<const TypeInfo*> argTypes;
    if (auto* argList = ctx->argList()) {
        for (auto* exprCtx : argList->expression())
            argTypes.push_back(resolveExprTypeInfo(exprCtx));
    }

    // ── Generic function templates (checked before builtins to allow override) ──
    {
        std::vector<const TypeInfo*> argTypes;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                argTypes.push_back(resolveExprTypeInfo(exprCtx));
        }

        auto fit = genericFuncTemplates_.find(funcName);
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
            if (argCountOk) {
                auto inferred = inferGenericTypeArgs(fit->second.typeParams, formalParams, argTypes);
                if (inferred) {
                    if (auto* genericFn = instantiateGenericFunc(funcName, fit->second, *inferred)) {
                        auto fnName = genericFn->getName().str();
                        auto vit = variadicFunctions_.find(fnName);
                        if (vit != variadicFunctions_.end()) {
                            // Typed variadic: pack args at call site
                            auto& vfInfo = vit->second;
                            size_t varIdx = vfInfo.variadicParamIdx;
                            auto* elemTy = vfInfo.elementType->toLLVMType(*context_, module_->getDataLayout());
                            auto* fnTy = genericFn->getFunctionType();
                            std::vector<llvm::Value*> callArgs;
                            std::vector<llvm::Value*> variadicArgs;
                            if (auto* argList = ctx->argList()) {
                                for (size_t ai = 0; ai < argList->expression().size(); ai++) {
                                    auto* argVal = castValue(visit(argList->expression(ai)));
                                    if (ai < varIdx) {
                                        if (argVal->getType() != fnTy->getParamType(ai)) {
                                            if (argVal->getType()->isIntegerTy() && fnTy->getParamType(ai)->isIntegerTy()) {
                                                bool srcSigned = ai < argTypes.size() && argTypes[ai] && argTypes[ai]->isSigned;
                                                argVal = builder_->CreateIntCast(argVal, fnTy->getParamType(ai), srcSigned);
                                            } else if (argVal->getType()->isFloatingPointTy() && fnTy->getParamType(ai)->isFloatingPointTy())
                                                argVal = builder_->CreateFPCast(argVal, fnTy->getParamType(ai));
                                        }
                                        callArgs.push_back(argVal);
                                    } else {
                                        if (argVal->getType() != elemTy) {
                                            if (argVal->getType()->isIntegerTy() && elemTy->isIntegerTy())
                                                argVal = builder_->CreateIntCast(argVal, elemTy, vfInfo.elementType->isSigned);
                                            else if (argVal->getType()->isFloatingPointTy() && elemTy->isFloatingPointTy())
                                                argVal = builder_->CreateFPCast(argVal, elemTy);
                                        }
                                        variadicArgs.push_back(argVal);
                                    }
                                }
                            }
                            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
                            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                            if (variadicArgs.empty()) {
                                callArgs.push_back(llvm::ConstantPointerNull::get(ptrTy));
                                callArgs.push_back(llvm::ConstantInt::get(i64Ty, 0));
                            } else {
                                auto* arrTy = llvm::ArrayType::get(elemTy, variadicArgs.size());
                                auto* alloca = builder_->CreateAlloca(arrTy, nullptr, "variadic.pack");
                                for (size_t vi = 0; vi < variadicArgs.size(); vi++) {
                                    auto* gep = builder_->CreateGEP(arrTy, alloca, {
                                        llvm::ConstantInt::get(i64Ty, 0),
                                        llvm::ConstantInt::get(i64Ty, static_cast<uint64_t>(vi))
                                    });
                                    builder_->CreateStore(variadicArgs[vi], gep);
                                }
                                callArgs.push_back(builder_->CreateGEP(arrTy, alloca, {
                                    llvm::ConstantInt::get(i64Ty, 0),
                                    llvm::ConstantInt::get(i64Ty, 0)
                                }));
                                callArgs.push_back(llvm::ConstantInt::get(i64Ty, variadicArgs.size()));
                            }
                            builder_->CreateCall(genericFn, callArgs);
                            if (ctx->argList()) {
                                auto exprs = ctx->argList()->expression();
                                for (size_t i = 0; i < exprs.size(); i++)
                                    cleanupTempArg(exprs[i], callArgs.size() > i ? callArgs[i] : nullptr);
                            }
                        } else {
                            // Regular (non-variadic) generic call
                            std::vector<llvm::Value*> args;
                            if (auto* argList = ctx->argList()) {
                                auto* fnTy = genericFn->getFunctionType();
                                size_t paramIdx = 0;
                                for (auto* exprCtx : argList->expression()) {
                                    auto* argVal = castValue(visit(exprCtx));
                                    if (paramIdx < fnTy->getNumParams()) {
                                        auto* paramTy = fnTy->getParamType(paramIdx);
                                        if (argVal->getType() != paramTy) {
                                            if (argVal->getType()->isIntegerTy() && paramTy->isIntegerTy()) {
                                                bool srcSigned = paramIdx < argTypes.size() && argTypes[paramIdx] && argTypes[paramIdx]->isSigned;
                                                argVal = builder_->CreateIntCast(argVal, paramTy, srcSigned);
                                            } else if (argVal->getType()->isFloatingPointTy() && paramTy->isFloatingPointTy())
                                                argVal = builder_->CreateFPCast(argVal, paramTy);
                                        }
                                    }
                                    args.push_back(argVal);
                                    paramIdx++;
                                }
                            }
                            builder_->CreateCall(genericFn, args);
                            if (ctx->argList()) {
                                auto exprs = ctx->argList()->expression();
                                for (size_t i = 0; i < exprs.size() && i < args.size(); i++)
                                    cleanupTempArg(exprs[i], args[i]);
                            }
                        }
                        return {};
                    }
                }
            }
        }
    }

    // ── Global builtins (always available, no import required) ──────────
    if (globalBuiltins_.count(funcName)) {
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* voidTy  = llvm::Type::getVoidTy(*context_);
        auto* i32Ty   = llvm::Type::getInt32Ty(*context_);
        auto* i1Ty    = llvm::Type::getInt1Ty(*context_);

        // Collect argument values
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }

        if (funcName == "exit") {
            auto* arg = args.empty()
                ? llvm::ConstantInt::get(i32Ty, 0)
                : builder_->CreateIntCast(args[0], i32Ty, true, "exit_code");
            auto callee = declareBuiltin("lucis_exit", voidTy, {i32Ty});
            builder_->CreateCall(callee, {arg});
        } else if (funcName == "panic") {
            if (!requireArgs(funcName, args, 1)) return {};
            auto* strPtr = builder_->CreateExtractValue(args[0], 0, "panic_ptr");
            auto* strLen = builder_->CreateExtractValue(args[0], 1, "panic_len");
            auto callee = declareBuiltin("lucis_panic", voidTy, {ptrTy, usizeTy});
            builder_->CreateCall(callee, {strPtr, strLen});
        } else if (funcName == "assert") {
            if (!requireArgs(funcName, args, 1)) return {};
            auto* cond = args[0];
            if (!cond->getType()->isIntegerTy(32))
                cond = builder_->CreateZExt(cond, i32Ty, "assert_cond");
            // Inject file name and line number at compile time
            auto fileName = module_->getName().str();
            auto* fileGlobal = builder_->CreateGlobalString(fileName, ".assert_file", 0, module_);
            auto* fileLen = llvm::ConstantInt::get(usizeTy, fileName.size());
            auto* lineNo  = llvm::ConstantInt::get(i32Ty, ctx->getStart()->getLine());
            auto callee = declareBuiltin("lucis_assert", voidTy,
                                         {i32Ty, ptrTy, usizeTy, i32Ty});
            builder_->CreateCall(callee, {cond, fileGlobal, fileLen, lineNo});
        } else if (funcName == "assertMsg") {
            if (!requireArgs(funcName, args, 2)) return {};
            auto* cond = args[0];
            if (!cond->getType()->isIntegerTy(32))
                cond = builder_->CreateZExt(cond, i32Ty, "assertmsg_cond");
            auto* msgPtr = builder_->CreateExtractValue(args[1], 0, "assertmsg_ptr");
            auto* msgLen = builder_->CreateExtractValue(args[1], 1, "assertmsg_len");
            auto callee = declareBuiltin("lucis_assertMsg", voidTy,
                                         {i32Ty, ptrTy, usizeTy});
            builder_->CreateCall(callee, {cond, msgPtr, msgLen});
        } else if (funcName == "unreachable") {
            auto fileName = module_->getName().str();
            auto* fileGlobal = builder_->CreateGlobalString(fileName, ".unreach_file", 0, module_);
            auto* fileLen = llvm::ConstantInt::get(usizeTy, fileName.size());
            auto* lineNo  = llvm::ConstantInt::get(i32Ty, ctx->getStart()->getLine());
            auto callee = declareBuiltin("lucis_unreachable", voidTy,
                                         {ptrTy, usizeTy, i32Ty});
            builder_->CreateCall(callee, {fileGlobal, fileLen, lineNo});
        } else if (funcName == "freeStr") {
            if (!requireArgs(funcName, args, 1)) return {};
            auto* strPtr = builder_->CreateExtractValue(args[0], 0, "freeStr_ptr");
            auto* strLen = builder_->CreateExtractValue(args[0], 1, "freeStr_len");
            auto callee = declareBuiltin("lucis_freeStr", voidTy, {ptrTy, usizeTy});
            builder_->CreateCall(callee, {strPtr, strLen});
            if (ctx->argList() && !ctx->argList()->expression().empty())
                consumeExprIfOwnedLocal(ctx->argList()->expression()[0]);
        }

        if (funcName != "freeStr" && ctx->argList()) {
            auto exprs = ctx->argList()->expression();
            for (size_t i = 0; i < exprs.size() && i < args.size(); i++)
                cleanupTempArg(exprs[i], args[i]);
        }

        return {};
    }

    // ── print/println/eprint/eprintln (imported from std::log) ─────
    if ((funcName == "print" || funcName == "println" ||
         funcName == "eprint" || funcName == "eprintln") &&
        (userImports_.count(funcName) || imports_.isImported(funcName))) {
      std::vector<llvm::Value*> args;
      std::vector<LucisParser::ExpressionContext*> argExprs;
      if (auto* argList = ctx->argList()) {
        for (auto* exprCtx : argList->expression()) {
          argExprs.push_back(exprCtx);
          args.push_back(castValue(visit(exprCtx)));
        }
      }

      auto* voidTy = llvm::Type::getVoidTy(*context_);
      auto* ptrTy  = llvm::PointerType::getUnqual(*context_);
      auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);

      for (size_t ai = 0; ai < args.size(); ai++) {
        auto* arg = args[ai];
        auto* argType = arg->getType();
        if (ai < argExprs.size() && resolveExprArrayDims(argExprs[ai]) > 0) {
          std::cerr << "lucis: function '" << funcName
                    << "' does not accept array arguments directly; "
                       "use '.toString()' or '.join(...)' before printing\n";
          continue;
        }

        const TypeInfo* argTypeInfo = nullptr;
        if (auto* load = llvm::dyn_cast<llvm::LoadInst>(arg)) {
          auto* src = load->getPointerOperand();
          for (auto& [vname, info] : locals_) {
            if (info.alloca == src) { argTypeInfo = info.typeInfo; break; }
          }
          if (argTypeInfo && argTypeInfo->kind == TypeKind::Union)
            argTypeInfo = nullptr;
        }

        bool isString = (argTypeInfo && argTypeInfo->kind == TypeKind::String)
                     || (!argTypeInfo && argType->isStructTy());
        if (isString) {
          auto* strPtr = builder_->CreateExtractValue(arg, 0, "str_ptr");
          auto* strLen = builder_->CreateExtractValue(arg, 1, "str_len");
          auto callee = declareBuiltin("lucis_" + funcName + "_str", voidTy, {ptrTy, usizeTy});
          builder_->CreateCall(callee, {strPtr, strLen});
          continue;
        }

        std::string suffix;
        if (argTypeInfo && !argTypeInfo->builtinSuffix.empty())
          suffix = argTypeInfo->builtinSuffix;
        else if (!argTypeInfo && ai < argExprs.size()) {
          auto* e = argExprs[ai];
          while (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(e))
            e = paren->expression();
          if (auto* cast = dynamic_cast<LucisParser::CastExprContext*>(e)) {
            auto* castTI = resolveTypeInfo(cast->typeSpec());
            if (castTI && !castTI->builtinSuffix.empty())
              suffix = castTI->builtinSuffix;
          } else if (dynamic_cast<LucisParser::CharLitExprContext*>(e)) {
            suffix = "char";
          }
        }
        if (suffix.empty()) {
          bool isUnsigned = argTypeInfo && !argTypeInfo->isSigned;
          if (argType->isIntegerTy(1))      suffix = "bool";
          else if (argType->isIntegerTy(8))      suffix = isUnsigned ? "u8" : "i8";
          else if (argType->isIntegerTy(16))     suffix = isUnsigned ? "u16" : "i16";
          else if (argType->isIntegerTy(32))     suffix = isUnsigned ? "u32" : "i32";
          else if (argType->isIntegerTy(64))     suffix = isUnsigned ? "u64" : "i64";
          else if (argType->isIntegerTy(128))    suffix = isUnsigned ? "u128" : "i128";
          else if (argType->isFloatTy())         suffix = "f32";
          else if (argType->isDoubleTy())        suffix = "f64";
          else if (argType->isPointerTy())       suffix = "ptr";
          else                                   suffix = "i32";
        }

        auto cFuncName = "lucis_" + funcName + "_" + suffix;
        llvm::Value* callArg = arg;
        llvm::Type* paramType = argType;
        if (argType->isIntegerTy(256)) {
          if (suffix == "i128")
            paramType = llvm::Type::getInt128Ty(*context_);
          else if (suffix == "i64")
            paramType = llvm::Type::getInt64Ty(*context_);
          else
            paramType = llvm::Type::getInt32Ty(*context_);
          callArg = builder_->CreateTrunc(arg, paramType);
        }

        auto callee = declareBuiltin(cFuncName, voidTy, {paramType});
        builder_->CreateCall(callee, {callArg});
      }

      if (ctx->argList()) {
        auto exprs = ctx->argList()->expression();
        for (size_t i = 0; i < exprs.size() && i < args.size(); i++)
          cleanupTempArg(exprs[i], args[i]);
      }

      return {};
    }

    if (auto fit = genericFuncTemplates_.find(funcName);
        fit != genericFuncTemplates_.end()) {
        std::vector<LucisParser::ParamContext*> formalParams;
        if (auto* paramList = fit->second.decl->paramList())
            formalParams = paramList->param();

        auto inferred = inferGenericTypeArgs(fit->second.typeParams, formalParams, argTypes);
        if (inferred) {
            if (auto* genericFn = instantiateGenericFunc(funcName, fit->second, *inferred)) {
                std::vector<llvm::Value*> args;
                if (auto* argList = ctx->argList()) {
                    auto* fnType = genericFn->getFunctionType();
                    size_t paramIdx = 0;
                    for (auto* exprCtx : argList->expression()) {
                        auto* argVal = castValue(visit(exprCtx));
                        if (paramIdx < fnType->getNumParams()) {
                            auto* paramTy = fnType->getParamType(paramIdx);
                            if (argVal->getType() != paramTy) {
                                if (argVal->getType()->isIntegerTy() && paramTy->isIntegerTy())
                                    argVal = builder_->CreateIntCast(argVal, paramTy, true);
                                else if (argVal->getType()->isFloatingPointTy() && paramTy->isFloatingPointTy()) {
                                    if (argVal->getType()->getPrimitiveSizeInBits() > paramTy->getPrimitiveSizeInBits())
                                        argVal = builder_->CreateFPTrunc(argVal, paramTy);
                                    else
                                        argVal = builder_->CreateFPExt(argVal, paramTy);
                                }
                            }
                        }
                        args.push_back(argVal);
                        paramIdx++;
                    }
                }
                builder_->CreateCall(genericFn, args);

                if (ctx->argList()) {
                    auto exprs = ctx->argList()->expression();
                    for (size_t i = 0; i < exprs.size() && i < args.size(); i++)
                        cleanupTempArg(exprs[i], args[i]);
                }

                return {};
            }
        }
    }

    // ── User-defined or extern C function called as statement ──────
    auto resolvedName = resolveCallTarget(funcName);
    auto* userFn = module_->getFunction(resolvedName);

    // Lazy declare C function from #include bindings
    if (!userFn)
        userFn = declareCFunction(funcName);

    if (userFn) {
        std::vector<llvm::Value*> rawArgs;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                rawArgs.push_back(castValue(visit(exprCtx)));
        }

        // ── C extern variadic function (e.g. printf) ───────────────
        if (externCFunctions_.count(resolvedName) && userFn->isVarArg()) {
            auto* fnType = userFn->getFunctionType();
            unsigned fixedParams = fnType->getNumParams();

            for (size_t i = 0; i < rawArgs.size(); i++) {
                auto* a = rawArgs[i];
                if (i < fixedParams) {
                    // Fixed params: coerce to declared type
                    if (a->getType() != fnType->getParamType(i)) {
                        if (a->getType()->isIntegerTy() && fnType->getParamType(i)->isIntegerTy())
                            a = builder_->CreateIntCast(a, fnType->getParamType(i), true);
                        else if (a->getType()->isFloatingPointTy() && fnType->getParamType(i)->isFloatingPointTy())
                            a = builder_->CreateFPExt(a, fnType->getParamType(i));
                    }
                } else {
                    // Variadic args: apply C default argument promotions
                    if (a->getType()->isFloatTy())
                        a = builder_->CreateFPExt(a, llvm::Type::getDoubleTy(*context_), "c_promote_f64");
                    else if (a->getType()->isIntegerTy()) {
                        unsigned bw = a->getType()->getIntegerBitWidth();
                        if (bw < 32) {
                            if (bw == 1)
                                a = builder_->CreateZExt(a, llvm::Type::getInt32Ty(*context_), "c_promote_bool");
                            else
                                a = builder_->CreateSExt(a, llvm::Type::getInt32Ty(*context_), "c_promote_i32");
                        }
                        else if (bw > 64)
                            a = builder_->CreateTrunc(a, llvm::Type::getInt32Ty(*context_), "c_trunc_i32");
                    } else if (a->getType()->isArrayTy()) {
                        // Array decay: pass pointer to first element for C variadics
                        llvm::Value* srcAlloca = nullptr;
                        if (auto* li = llvm::dyn_cast<llvm::LoadInst>(a))
                            srcAlloca = li->getPointerOperand();
                        if (!srcAlloca) {
                            srcAlloca = builder_->CreateAlloca(a->getType(), nullptr, "arr.decay");
                            builder_->CreateStore(a, srcAlloca);
                        }
                        auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
                        a = builder_->CreateGEP(a->getType(), srcAlloca, {zero, zero}, "arr.ptr");
                    } else if (a->getType()->isStructTy()) {
                        // Lucis string { ptr, i64 } → extract ptr for C variadics
                        auto* st = llvm::cast<llvm::StructType>(a->getType());
                        if (st->getNumElements() == 2 &&
                            st->getElementType(0)->isPointerTy() &&
                            st->getElementType(1)->isIntegerTy())
                            a = builder_->CreateExtractValue(a, 0, "str_to_cptr");
                    }
                }
                rawArgs[i] = a;
            }
            builder_->CreateCall(userFn, rawArgs);
            return {};
        }

        auto vit = variadicFunctions_.find(resolvedName);
        if (vit != variadicFunctions_.end()) {
            auto& vfInfo = vit->second;
            size_t varIdx = vfInfo.variadicParamIdx;
            if (!vfInfo.elementType) {
                std::cerr << "lucis: internal error: variadic function missing elementType\n";
                return {};
            }
            auto* elemTy = vfInfo.elementType->toLLVMType(*context_, module_->getDataLayout());
            auto* fnType = userFn->getFunctionType();

            std::vector<llvm::Value*> callArgs;
            std::vector<llvm::Value*> variadicArgs;

            for (size_t i = 0; i < rawArgs.size(); i++) {
                auto* a = rawArgs[i];
                if (i < varIdx) {
                    if (a->getType() != fnType->getParamType(i)) {
                        if (a->getType()->isIntegerTy() && fnType->getParamType(i)->isIntegerTy())
                            a = builder_->CreateIntCast(a, fnType->getParamType(i), true);
                        else if (a->getType()->isFloatingPointTy() && fnType->getParamType(i)->isFloatingPointTy()) {
                            if (a->getType()->getPrimitiveSizeInBits() > fnType->getParamType(i)->getPrimitiveSizeInBits())
                                a = builder_->CreateFPTrunc(a, fnType->getParamType(i));
                            else
                                a = builder_->CreateFPExt(a, fnType->getParamType(i));
                        }
                    }
                    callArgs.push_back(a);
                } else {
                    if (a->getType() != elemTy) {
                        if (a->getType()->isIntegerTy() && elemTy->isIntegerTy())
                            a = builder_->CreateIntCast(a, elemTy, vfInfo.elementType->isSigned);
                        else if (a->getType()->isFloatingPointTy() && elemTy->isFloatingPointTy()) {
                            if (a->getType()->getPrimitiveSizeInBits() > elemTy->getPrimitiveSizeInBits())
                                a = builder_->CreateFPTrunc(a, elemTy);
                            else
                                a = builder_->CreateFPExt(a, elemTy);
                        }
                    }
                    variadicArgs.push_back(a);
                }
            }

            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
            auto* ptrTy = llvm::PointerType::getUnqual(*context_);

            if (variadicArgs.empty()) {
                callArgs.push_back(llvm::ConstantPointerNull::get(ptrTy));
                callArgs.push_back(llvm::ConstantInt::get(i64Ty, 0));
            } else {
                auto* arrTy = llvm::ArrayType::get(elemTy, variadicArgs.size());
                auto* arrAlloca = builder_->CreateAlloca(arrTy, nullptr, "variadic.pack");
                auto* zero = llvm::ConstantInt::get(i64Ty, 0);
                for (size_t j = 0; j < variadicArgs.size(); j++) {
                    auto* idx = llvm::ConstantInt::get(i64Ty, j);
                    auto* gep = builder_->CreateGEP(arrTy, arrAlloca, {zero, idx});
                    builder_->CreateStore(variadicArgs[j], gep);
                }
                callArgs.push_back(arrAlloca);
                callArgs.push_back(llvm::ConstantInt::get(i64Ty, variadicArgs.size()));
            }

            builder_->CreateCall(userFn, callArgs);
        } else {
            auto* fnType = userFn->getFunctionType();

            // Check if this C function needs ABI struct coercion
            auto abiIt = cabiInfos_.find(userFn->getName().str());
            bool needsABI = (abiIt != cabiInfos_.end());
            CABIInfo* abi = needsABI ? &abiIt->second : nullptr;

            std::vector<llvm::Value*> args;

            // Sret: allocate space for large struct return, pass as hidden first arg
            llvm::AllocaInst* sretAlloca = nullptr;
            if (abi && abi->returnInfo.kind == ABIArgKind::Indirect) {
                sretAlloca = builder_->CreateAlloca(abi->originalRetType, nullptr, "sret");
                args.push_back(sretAlloca);
            }

            for (size_t i = 0; i < rawArgs.size(); i++) {
                auto* argVal = rawArgs[i];

                // ABI struct coercion for C functions
                if (abi && i < abi->paramInfos.size()) {
                    auto& pi = abi->paramInfos[i];
                    if (pi.kind == ABIArgKind::Coerce && argVal->getType()->isStructTy()) {
                        auto* tmp = builder_->CreateAlloca(argVal->getType(), nullptr, "abi.coerce");
                        builder_->CreateStore(argVal, tmp);
                        argVal = builder_->CreateLoad(pi.coercedType, tmp, "abi.coerced");
                        args.push_back(argVal);
                        continue;
                    }
                    if (pi.kind == ABIArgKind::Expand && argVal->getType()->isStructTy()) {
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
                        auto* tmp = builder_->CreateAlloca(argVal->getType(), nullptr, "abi.byval");
                        builder_->CreateStore(argVal, tmp);
                        args.push_back(tmp);
                        continue;
                    }
                }

                // Standard type coercion
                unsigned llvmIdx = static_cast<unsigned>(i);
                if (sretAlloca) llvmIdx++;
                if (llvmIdx < fnType->getNumParams() && argVal->getType() != fnType->getParamType(llvmIdx)) {
                    auto* expectedTy = fnType->getParamType(llvmIdx);
                    if (argVal->getType()->isArrayTy() && expectedTy->isStructTy()) {
                        // Array-to-slice lowering for []T-style params:
                        // pass {ptr to first element, len}. Mirrors the
                        // same conversion in visitFnCallExpr (IRGen_call.cpp).
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
                            auto* lenField = llvm::ConstantInt::get(
                                llvm::cast<llvm::IntegerType>(lenFieldTy),
                                srcArrTy->getNumElements());

                            llvm::Value* slice = llvm::UndefValue::get(dstST);
                            slice = builder_->CreateInsertValue(slice, ptrField, {0}, "arr.slice.ins.ptr");
                            slice = builder_->CreateInsertValue(slice, lenField, {1}, "arr.slice.ins.len");
                            argVal = slice;

                            if (deadLoad && deadLoad->use_empty())
                                deadLoad->eraseFromParent();
                        }
                    } else if (argVal->getType()->isArrayTy() && expectedTy->isPointerTy()) {
                        // Reuse the source alloca if the value came from a load —
                        // avoids a full array copy that would hang the LLVM backend.
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
                        argVal = builder_->CreateGEP(argVal->getType(), srcAlloca, {zero, zero}, "arr.ptr");
                        if (deadLoad && deadLoad->use_empty())
                            deadLoad->eraseFromParent();
                    } else if (argVal->getType()->isIntegerTy() && expectedTy->isIntegerTy())
                        argVal = builder_->CreateIntCast(argVal, expectedTy, true);
                    else if (argVal->getType()->isFloatingPointTy() && expectedTy->isFloatingPointTy()) {
                        if (argVal->getType()->getPrimitiveSizeInBits() > expectedTy->getPrimitiveSizeInBits())
                            argVal = builder_->CreateFPTrunc(argVal, expectedTy);
                        else
                            argVal = builder_->CreateFPExt(argVal, expectedTy);
                    }
                }

                // For untyped variadic args in user-defined functions,
                // apply default C argument promotions (float -> double, small ints -> i32)
                if (userFn->isVarArg() && i >= fnType->getNumParams()) {
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
            builder_->CreateCall(userFn, args);
        }

        if (ctx->argList()) {
            auto exprs = ctx->argList()->expression();
            for (size_t i = 0; i < exprs.size() && i < rawArgs.size(); i++)
                cleanupTempArg(exprs[i], rawArgs[i]);
        }

        return {};
    }

    if (!imports_.isImported(funcName)) {
        std::cerr << "lucis: call to undeclared function '" << funcName
                  << "'. Did you forget 'use std::log::" << funcName << ";'?\n";
        return {};
    }

    // ── dbg() as statement: print debug info to stderr ──────────────────
    if (funcName == "dbg") {
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* i32Ty   = llvm::Type::getInt32Ty(*context_);

        // Inject file name and line number
        auto fileName = module_->getName().str();
        auto* fileGlobal = builder_->CreateGlobalString(fileName, ".dbg_file", 0, module_);
        auto* fileLen = llvm::ConstantInt::get(usizeTy, fileName.size());
        auto* lineNo  = llvm::ConstantInt::get(i32Ty, ctx->getStart()->getLine());

        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }

        for (auto* arg : args) {
            auto* argType = arg->getType();
            const TypeInfo* argTypeInfo = nullptr;
            if (auto* load = llvm::dyn_cast<llvm::LoadInst>(arg)) {
                auto* src = load->getPointerOperand();
                for (auto& [vname, info] : locals_) {
                    if (info.alloca == src) { argTypeInfo = info.typeInfo; break; }
                }
                if (argTypeInfo && argTypeInfo->kind == TypeKind::Union)
                    argTypeInfo = nullptr;
            }

            bool isString = (argTypeInfo && argTypeInfo->kind == TypeKind::String)
                         || (!argTypeInfo && argType->isStructTy());

            if (isString) {
                auto* strPtr = builder_->CreateExtractValue(arg, 0, "dbg_str_ptr");
                auto* strLen = builder_->CreateExtractValue(arg, 1, "dbg_str_len");
                auto callee = declareBuiltin("lucis_dbg_str",
                    llvm::Type::getVoidTy(*context_),
                    {ptrTy, usizeTy, i32Ty, ptrTy, usizeTy});
                builder_->CreateCall(callee, {fileGlobal, fileLen, lineNo, strPtr, strLen});
                continue;
            }

            std::string suffix;
            if (argTypeInfo)                      suffix = argTypeInfo->builtinSuffix;
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
        }
        return {};
    }

    // ── std::io void functions as statement ─────────────────────────────
    if (funcName == "flush") {
        auto callee = declareBuiltin("lucis_flush",
            llvm::Type::getVoidTy(*context_), {});
        builder_->CreateCall(callee, {});
        return {};
    }
    if (funcName == "flushErr") {
        auto callee = declareBuiltin("lucis_flushErr",
            llvm::Type::getVoidTy(*context_), {});
        builder_->CreateCall(callee, {});
        return {};
    }

    // ── std::mem void functions as statement ─────────────────────────
    if (funcName == "free") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }
        auto* ptrTy  = llvm::PointerType::getUnqual(*context_);
        auto callee = declareBuiltin("lucis_free",
            llvm::Type::getVoidTy(*context_), {ptrTy});
        if (!requireArgs(funcName, args, 1)) return {};
        builder_->CreateCall(callee, {args[0]});
        return {};
    }
    if (funcName == "copy") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }
        if (!requireArgs(funcName, args, 3)) return {};
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* n = args[2];
        if (n->getType() != usizeTy)
            n = builder_->CreateIntCast(n, usizeTy, false);
        auto callee = declareBuiltin("lucis_copy",
            llvm::Type::getVoidTy(*context_), {ptrTy, ptrTy, usizeTy});
        builder_->CreateCall(callee, {args[0], args[1], n});
        return {};
    }
    if (funcName == "move") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }
        if (!requireArgs(funcName, args, 3)) return {};
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* n = args[2];
        if (n->getType() != usizeTy)
            n = builder_->CreateIntCast(n, usizeTy, false);
        auto callee = declareBuiltin("lucis_move",
            llvm::Type::getVoidTy(*context_), {ptrTy, ptrTy, usizeTy});
        builder_->CreateCall(callee, {args[0], args[1], n});
        return {};
    }
    if (funcName == "set") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }
        if (!requireArgs(funcName, args, 3)) return {};
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* i8Ty    = llvm::Type::getInt8Ty(*context_);
        auto* val = args[1];
        if (val->getType() != i8Ty)
            val = builder_->CreateIntCast(val, i8Ty, false);
        auto* n = args[2];
        if (n->getType() != usizeTy)
            n = builder_->CreateIntCast(n, usizeTy, false);
        auto callee = declareBuiltin("lucis_set",
            llvm::Type::getVoidTy(*context_), {ptrTy, i8Ty, usizeTy});
        builder_->CreateCall(callee, {args[0], val, n});
        return {};
    }
    if (funcName == "zero") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }
        if (!requireArgs(funcName, args, 2)) return {};
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* n = args[1];
        if (n->getType() != usizeTy)
            n = builder_->CreateIntCast(n, usizeTy, false);
        auto callee = declareBuiltin("lucis_zero",
            llvm::Type::getVoidTy(*context_), {ptrTy, usizeTy});
        builder_->CreateCall(callee, {args[0], n});
        return {};
    }

    // ── std::random void functions as statement ─────────────────────
    if (funcName == "seed") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }
        auto* u64Ty = llvm::Type::getInt64Ty(*context_);
        if (!requireArgs(funcName, args, 1)) return {};
        auto* s = args[0];
        if (s->getType() != u64Ty)
            s = builder_->CreateIntCast(s, u64Ty, false);
        auto callee = declareBuiltin("lucis_seed",
            llvm::Type::getVoidTy(*context_), {u64Ty});
        builder_->CreateCall(callee, {s});
        return {};
    }
    if (funcName == "seedTime") {
        auto callee = declareBuiltin("lucis_seedTime",
            llvm::Type::getVoidTy(*context_), {});
        builder_->CreateCall(callee, {});
        return {};
    }

    // ── std::time void functions as statement ───────────────────────
    if (funcName == "sleep") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }
        auto* u64Ty = llvm::Type::getInt64Ty(*context_);
        if (!requireArgs(funcName, args, 1)) return {};
        auto* ms = args[0];
        if (ms->getType() != u64Ty)
            ms = builder_->CreateIntCast(ms, u64Ty, false);
        auto callee = declareBuiltin("lucis_sleep",
            llvm::Type::getVoidTy(*context_), {u64Ty});
        builder_->CreateCall(callee, {ms});
        return {};
    }
    if (funcName == "sleepMicros") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }
        auto* u64Ty = llvm::Type::getInt64Ty(*context_);
        if (!requireArgs(funcName, args, 1)) return {};
        auto* us = args[0];
        if (us->getType() != u64Ty)
            us = builder_->CreateIntCast(us, u64Ty, false);
        auto callee = declareBuiltin("lucis_sleepMicros",
            llvm::Type::getVoidTy(*context_), {u64Ty});
        builder_->CreateCall(callee, {us});
        return {};
    }

    // ── std::fs void functions as statement ─────────────────────────
    if (funcName == "writeFile" || funcName == "appendFile") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* voidTy  = llvm::Type::getVoidTy(*context_);

        if (!requireArgs(funcName, args, 2)) return {};
        auto* pathPtr = builder_->CreateExtractValue(args[0], 0, "fs_path_ptr");
        auto* pathLen = builder_->CreateExtractValue(args[0], 1, "fs_path_len");
        auto* dataPtr = builder_->CreateExtractValue(args[1], 0, "fs_data_ptr");
        auto* dataLen = builder_->CreateExtractValue(args[1], 1, "fs_data_len");

        auto callee2 = declareBuiltin("lucis_" + funcName, voidTy,
            {ptrTy, usizeTy, ptrTy, usizeTy});
        builder_->CreateCall(callee2, {pathPtr, pathLen, dataPtr, dataLen});
        return {};
    }

    // ── std::fs bool-returning functions called as statement (discard result) ─
    {
        static const std::unordered_set<std::string> fsBoolStmt1 = {
            "exists", "isFile", "isDir",
            "remove", "removeDir",
            "mkdir", "mkdirAll", "setCwd"
        };
        if (fsBoolStmt1.count(funcName)) {
            std::vector<llvm::Value*> args;
            if (auto* argList = ctx->argList()) {
                for (auto* exprCtx : argList->expression())
                    args.push_back(castValue(visit(exprCtx)));
            }
            if (!requireArgs(funcName, args, 1)) return {};
            auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto* i32Ty   = llvm::Type::getInt32Ty(*context_);

            auto* pathPtr = builder_->CreateExtractValue(args[0], 0, "fs_path_ptr");
            auto* pathLen = builder_->CreateExtractValue(args[0], 1, "fs_path_len");
            auto callee2 = declareBuiltin("lucis_" + funcName, i32Ty,
                {ptrTy, usizeTy});
            builder_->CreateCall(callee2, {pathPtr, pathLen});
            return {};
        }
    }
    if (funcName == "rename") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }
        if (!requireArgs(funcName, args, 2)) return {};
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* i32Ty   = llvm::Type::getInt32Ty(*context_);

        auto* fromPtr = builder_->CreateExtractValue(args[0], 0, "fs_from_ptr");
        auto* fromLen = builder_->CreateExtractValue(args[0], 1, "fs_from_len");
        auto* toPtr   = builder_->CreateExtractValue(args[1], 0, "fs_to_ptr");
        auto* toLen   = builder_->CreateExtractValue(args[1], 1, "fs_to_len");
        auto callee2 = declareBuiltin("lucis_fsRename", i32Ty,
            {ptrTy, usizeTy, ptrTy, usizeTy});
        builder_->CreateCall(callee2, {fromPtr, fromLen, toPtr, toLen});
        return {};
    }

    // ── std::process void functions as statement ────────────────────
    if (funcName == "abort") {
        auto callee2 = declareBuiltin("lucis_abort",
            llvm::Type::getVoidTy(*context_), {});
        builder_->CreateCall(callee2, {});
        builder_->CreateUnreachable();
        return {};
    }
    if (funcName == "setEnv") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }
        if (!requireArgs(funcName, args, 2)) return {};
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* voidTy  = llvm::Type::getVoidTy(*context_);

        auto* namePtr = builder_->CreateExtractValue(args[0], 0, "env_name_ptr");
        auto* nameLen = builder_->CreateExtractValue(args[0], 1, "env_name_len");
        auto* valPtr  = builder_->CreateExtractValue(args[1], 0, "env_val_ptr");
        auto* valLen  = builder_->CreateExtractValue(args[1], 1, "env_val_len");
        auto callee2 = declareBuiltin("lucis_setEnv", voidTy,
            {ptrTy, usizeTy, ptrTy, usizeTy});
        builder_->CreateCall(callee2, {namePtr, nameLen, valPtr, valLen});
        return {};
    }
    if (funcName == "exec") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList()) {
            for (auto* exprCtx : argList->expression())
                args.push_back(castValue(visit(exprCtx)));
        }
        if (!requireArgs(funcName, args, 1)) return {};
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* i32Ty   = llvm::Type::getInt32Ty(*context_);

        auto* cmdPtr = builder_->CreateExtractValue(args[0], 0, "cmd_ptr");
        auto* cmdLen = builder_->CreateExtractValue(args[0], 1, "cmd_len");
        auto callee2 = declareBuiltin("lucis_exec", i32Ty,
            {ptrTy, usizeTy});
        builder_->CreateCall(callee2, {cmdPtr, cmdLen});
        return {};
    }

    // ── std::test — assertEqual(T, T) as statement ─────────────────
    if (funcName == "assertEqual" || funcName == "assertNotEqual") {
        std::vector<llvm::Value*> tArgs;
        if (auto* argList = ctx->argList())
            for (auto* e : argList->expression())
                tArgs.push_back(castValue(visit(e)));
        if (!requireArgs(funcName, tArgs, 2)) return {};
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* voidTy  = llvm::Type::getVoidTy(*context_);
        auto* i8Ty    = llvm::Type::getInt8Ty(*context_);
        auto* i32Ty   = llvm::Type::getInt32Ty(*context_);
        auto* i64Ty   = llvm::Type::getInt64Ty(*context_);
        auto* f64Ty   = llvm::Type::getDoubleTy(*context_);
        auto* strTy   = llvm::StructType::get(*context_, {ptrTy, usizeTy});
        auto* i1Ty    = llvm::Type::getInt1Ty(*context_);
        std::string prefix = (funcName == "assertEqual")
            ? "lucis_assertEqual" : "lucis_assertNotEqual";
        auto* ty = tArgs[0]->getType();
        auto* rhs = tArgs[1];
        auto fileName = module_->getName().str();
        auto* fileGlobal = builder_->CreateGlobalString(fileName, ".test_assert_file", 0, module_);
        auto* fileLen = llvm::ConstantInt::get(usizeTy, fileName.size());
        auto* lineNo  = llvm::ConstantInt::get(i32Ty, ctx->getStart()->getLine());
        if (rhs->getType() != ty) {
            if (ty->isIntegerTy() && rhs->getType()->isIntegerTy()) {
                rhs = builder_->CreateIntCast(rhs, ty, true);
            } else if (ty->isDoubleTy() && rhs->getType()->isIntegerTy()) {
                rhs = builder_->CreateSIToFP(rhs, ty);
            } else if (ty->isIntegerTy() && rhs->getType()->isDoubleTy()) {
                rhs = builder_->CreateFPToSI(rhs, ty);
            }
        }
        if (ty == strTy) {
            std::vector<llvm::Value*> callArgs;
            callArgs.push_back(builder_->CreateExtractValue(tArgs[0], 0));
            callArgs.push_back(builder_->CreateExtractValue(tArgs[0], 1));
            callArgs.push_back(builder_->CreateExtractValue(rhs, 0));
            callArgs.push_back(builder_->CreateExtractValue(rhs, 1));
            callArgs.push_back(fileGlobal);
            callArgs.push_back(fileLen);
            callArgs.push_back(lineNo);
            auto callee = declareBuiltin(prefix + "Str", voidTy,
                {ptrTy, usizeTy, ptrTy, usizeTy, ptrTy, usizeTy, i32Ty});
            builder_->CreateCall(callee, callArgs);
        } else if (ty == i1Ty) {
            auto* a = builder_->CreateZExt(tArgs[0], i32Ty);
            auto* b = builder_->CreateZExt(rhs, i32Ty);
            auto callee = declareBuiltin(prefix + "Bool", voidTy,
                {i32Ty, i32Ty, ptrTy, usizeTy, i32Ty});
            builder_->CreateCall(callee, {a, b, fileGlobal, fileLen, lineNo});
        } else if (ty == i8Ty) {
            auto callee = declareBuiltin(prefix + "Char", voidTy,
                {i8Ty, i8Ty, ptrTy, usizeTy, i32Ty});
            builder_->CreateCall(callee, {tArgs[0], rhs, fileGlobal, fileLen, lineNo});
        } else if (ty->isDoubleTy()) {
            auto callee = declareBuiltin(prefix + "F64", voidTy,
                {f64Ty, f64Ty, ptrTy, usizeTy, i32Ty});
            builder_->CreateCall(callee, {tArgs[0], rhs, fileGlobal, fileLen, lineNo});
        } else {
            auto* a = builder_->CreateIntCast(tArgs[0], i64Ty, true);
            auto* b = builder_->CreateIntCast(rhs, i64Ty, true);
            auto callee = declareBuiltin(prefix + "I64", voidTy,
                {i64Ty, i64Ty, ptrTy, usizeTy, i32Ty});
            builder_->CreateCall(callee, {a, b, fileGlobal, fileLen, lineNo});
        }
        return {};
    }

    // ── std::test — assertTrue / assertFalse as statement ──────────
    if (funcName == "assertTrue" || funcName == "assertFalse") {
        std::vector<llvm::Value*> tArgs;
        if (auto* argList = ctx->argList())
            for (auto* e : argList->expression())
                tArgs.push_back(castValue(visit(e)));
        if (!requireArgs(funcName, tArgs, 1)) return {};
        auto* voidTy = llvm::Type::getVoidTy(*context_);
        auto* i32Ty  = llvm::Type::getInt32Ty(*context_);
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* cond = builder_->CreateZExt(tArgs[0], i32Ty);
        auto fileName = module_->getName().str();
        auto* fileGlobal = builder_->CreateGlobalString(fileName, ".test_assert_bool_file", 0, module_);
        auto* fileLen = llvm::ConstantInt::get(usizeTy, fileName.size());
        auto* lineNo  = llvm::ConstantInt::get(i32Ty, ctx->getStart()->getLine());
        std::string cName = (funcName == "assertTrue")
            ? "lucis_assertTrue" : "lucis_assertFalse";
        auto callee = declareBuiltin(cName, voidTy, {i32Ty, ptrTy, usizeTy, i32Ty});
        builder_->CreateCall(callee, {cond, fileGlobal, fileLen, lineNo});
        return {};
    }

    // ── std::test — assertGreater/Less/GreaterEq/LessEq as statement ─
    {
        static const std::unordered_map<std::string, std::string> cmpI64 = {
            {"assertGreater",   "lucis_assertGreaterI64"},
            {"assertLess",      "lucis_assertLessI64"},
            {"assertGreaterEq", "lucis_assertGreaterEqI64"},
            {"assertLessEq",    "lucis_assertLessEqI64"},
        };
        static const std::unordered_map<std::string, std::string> cmpF64 = {
            {"assertGreater",   "lucis_assertGreaterF64"},
            {"assertLess",      "lucis_assertLessF64"},
            {"assertGreaterEq", "lucis_assertGreaterEqF64"},
            {"assertLessEq",    "lucis_assertLessEqF64"},
        };
        auto itI = cmpI64.find(funcName);
        if (itI != cmpI64.end()) {
            std::vector<llvm::Value*> tArgs;
            if (auto* argList = ctx->argList())
                for (auto* e : argList->expression())
                    tArgs.push_back(castValue(visit(e)));
            if (!requireArgs(funcName, tArgs, 2)) return {};
            auto* voidTy = llvm::Type::getVoidTy(*context_);
            auto* i64Ty  = llvm::Type::getInt64Ty(*context_);
            auto* i32Ty  = llvm::Type::getInt32Ty(*context_);
            auto* f64Ty  = llvm::Type::getDoubleTy(*context_);
            auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto fileName = module_->getName().str();
            auto* fileGlobal = builder_->CreateGlobalString(fileName, ".test_cmp_file", 0, module_);
            auto* fileLen = llvm::ConstantInt::get(usizeTy, fileName.size());
            auto* lineNo  = llvm::ConstantInt::get(i32Ty, ctx->getStart()->getLine());
            auto* ty = tArgs[0]->getType();
            if (ty->isDoubleTy()) {
                auto callee = declareBuiltin(cmpF64.at(funcName), voidTy,
                    {f64Ty, f64Ty, ptrTy, usizeTy, i32Ty});
                builder_->CreateCall(callee, {tArgs[0], tArgs[1], fileGlobal, fileLen, lineNo});
            } else {
                auto* a = builder_->CreateIntCast(tArgs[0], i64Ty, true);
                auto* b = builder_->CreateIntCast(tArgs[1], i64Ty, true);
                auto callee = declareBuiltin(itI->second, voidTy,
                    {i64Ty, i64Ty, ptrTy, usizeTy, i32Ty});
                builder_->CreateCall(callee, {a, b, fileGlobal, fileLen, lineNo});
            }
            return {};
        }
    }

    // ── std::test — assertStringContains(string, string) as statement ─
    if (funcName == "assertStringContains") {
        std::vector<llvm::Value*> tArgs;
        if (auto* argList = ctx->argList())
            for (auto* e : argList->expression())
                tArgs.push_back(castValue(visit(e)));
        if (!requireArgs(funcName, tArgs, 2)) return {};
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* i32Ty   = llvm::Type::getInt32Ty(*context_);
        auto* voidTy  = llvm::Type::getVoidTy(*context_);
        auto fileName = module_->getName().str();
        auto* fileGlobal = builder_->CreateGlobalString(fileName, ".test_strcontains_file", 0, module_);
        auto* fileLen = llvm::ConstantInt::get(usizeTy, fileName.size());
        auto* lineNo  = llvm::ConstantInt::get(i32Ty, ctx->getStart()->getLine());
        std::vector<llvm::Value*> callArgs;
        callArgs.push_back(builder_->CreateExtractValue(tArgs[0], 0));
        callArgs.push_back(builder_->CreateExtractValue(tArgs[0], 1));
        callArgs.push_back(builder_->CreateExtractValue(tArgs[1], 0));
        callArgs.push_back(builder_->CreateExtractValue(tArgs[1], 1));
        callArgs.push_back(fileGlobal);
        callArgs.push_back(fileLen);
        callArgs.push_back(lineNo);
        auto callee = declareBuiltin("lucis_assertStringContains", voidTy,
            {ptrTy, usizeTy, ptrTy, usizeTy, ptrTy, usizeTy, i32Ty});
        builder_->CreateCall(callee, callArgs);
        return {};
    }

    // ── std::test — assertNear(f64, f64, f64) as statement ────────
    if (funcName == "assertNear") {
        std::vector<llvm::Value*> tArgs;
        if (auto* argList = ctx->argList())
            for (auto* e : argList->expression())
                tArgs.push_back(castValue(visit(e)));
        if (!requireArgs(funcName, tArgs, 3)) return {};
        auto* voidTy = llvm::Type::getVoidTy(*context_);
        auto* f64Ty  = llvm::Type::getDoubleTy(*context_);
        auto* i32Ty   = llvm::Type::getInt32Ty(*context_);
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto fileName = module_->getName().str();
        auto* fileGlobal = builder_->CreateGlobalString(fileName, ".test_near_file", 0, module_);
        auto* fileLen = llvm::ConstantInt::get(usizeTy, fileName.size());
        auto* lineNo  = llvm::ConstantInt::get(i32Ty, ctx->getStart()->getLine());
        auto callee = declareBuiltin("lucis_assertNear", voidTy,
            {f64Ty, f64Ty, f64Ty, ptrTy, usizeTy, i32Ty});
        builder_->CreateCall(callee, {tArgs[0], tArgs[1], tArgs[2], fileGlobal, fileLen, lineNo});
        return {};
    }

    // ── std::test — fail / skip / log (string) as statement ───────
    {
        static const std::unordered_map<std::string, std::string> testStrFuncs = {
            {"fail", "lucis_testFail"},
            {"skip", "lucis_testSkip"},
            {"log",  "lucis_testLog"},
        };
        auto tsIt = testStrFuncs.find(funcName);
        if (tsIt != testStrFuncs.end()) {
            std::vector<llvm::Value*> tArgs;
            if (auto* argList = ctx->argList())
                for (auto* e : argList->expression())
                    tArgs.push_back(castValue(visit(e)));
            if (!requireArgs(funcName, tArgs, 1)) return {};
            auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto* i32Ty   = llvm::Type::getInt32Ty(*context_);
            auto* voidTy  = llvm::Type::getVoidTy(*context_);
            auto fileName = module_->getName().str();
            auto* fileGlobal = builder_->CreateGlobalString(fileName, ".test_util_file", 0, module_);
            auto* fileLen = llvm::ConstantInt::get(usizeTy, fileName.size());
            auto* lineNo  = llvm::ConstantInt::get(i32Ty, ctx->getStart()->getLine());
            auto* strPtr = builder_->CreateExtractValue(tArgs[0], 0);
            auto* strLen = builder_->CreateExtractValue(tArgs[0], 1);
            auto callee = declareBuiltin(tsIt->second, voidTy, {ptrTy, usizeTy, ptrTy, usizeTy, i32Ty});
            builder_->CreateCall(callee, {strPtr, strLen, fileGlobal, fileLen, lineNo});
            return {};
        }
    }

    // ── std::net — void functions (close, setTimeout) ─────────────────
    if (funcName == "close") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList())
            for (auto* e : argList->expression())
                args.push_back(castValue(visit(e)));
        if (!requireArgs(funcName, args, 1)) return {};
        auto* i32Ty  = llvm::Type::getInt32Ty(*context_);
        auto* voidTy = llvm::Type::getVoidTy(*context_);
        auto* fd = builder_->CreateIntCast(args[0], i32Ty, true);
        auto callee = declareBuiltin("lucis_netClose", voidTy, {i32Ty});
        builder_->CreateCall(callee, {fd});
        return {};
    }
    if (funcName == "setTimeout") {
        std::vector<llvm::Value*> args;
        if (auto* argList = ctx->argList())
            for (auto* e : argList->expression())
                args.push_back(castValue(visit(e)));
        if (!requireArgs(funcName, args, 2)) return {};
        auto* i32Ty  = llvm::Type::getInt32Ty(*context_);
        auto* u64Ty  = llvm::Type::getInt64Ty(*context_);
        auto* voidTy = llvm::Type::getVoidTy(*context_);
        auto* fd = builder_->CreateIntCast(args[0], i32Ty, true);
        auto* ms = args[1];
        if (ms->getType() != u64Ty)
            ms = builder_->CreateIntCast(ms, u64Ty, false);
        auto callee = declareBuiltin("lucis_netSetTimeout", voidTy, {i32Ty, u64Ty});
        builder_->CreateCall(callee, {fd, ms});
        return {};
    }

    // ── std::net — value-returning functions used as statements ──────────
    {
        auto* voidTy = llvm::Type::getVoidTy(*context_);
        auto* i16Ty  = llvm::Type::getInt16Ty(*context_);
        auto* i32Ty  = llvm::Type::getInt32Ty(*context_);
        auto* i64Ty  = llvm::Type::getInt64Ty(*context_);
        auto* ptrTy  = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* strTy  = llvm::StructType::get(*context_, {ptrTy, usizeTy});

        // tcpConnect/tcpListen/udpBind: (string, uint16) → int32
        if (funcName == "tcpConnect" || funcName == "tcpListen" ||
            funcName == "udpBind") {
            std::vector<llvm::Value*> args;
            if (auto* argList = ctx->argList())
                for (auto* e : argList->expression())
                    args.push_back(castValue(visit(e)));
            if (!requireArgs(funcName, args, 2)) return {};
            std::vector<llvm::Value*> callArgs;
            callArgs.push_back(builder_->CreateExtractValue(args[0], 0));
            callArgs.push_back(builder_->CreateExtractValue(args[0], 1));
            auto* port = builder_->CreateIntCast(args[1], i16Ty, false);
            callArgs.push_back(port);
            std::string cName = "lucis_" + funcName;
            auto callee = declareBuiltin(cName, i32Ty, {ptrTy, usizeTy, i16Ty});
            builder_->CreateCall(callee, callArgs);
            return {};
        }
        // tcpAccept: (int32) → int32
        if (funcName == "tcpAccept") {
            std::vector<llvm::Value*> args;
            if (auto* argList = ctx->argList())
                for (auto* e : argList->expression())
                    args.push_back(castValue(visit(e)));
            if (!requireArgs(funcName, args, 1)) return {};
            auto* fd = builder_->CreateIntCast(args[0], i32Ty, true);
            auto callee = declareBuiltin("lucis_tcpAccept", i32Ty, {i32Ty});
            builder_->CreateCall(callee, {fd});
            return {};
        }
        // tcpSend: (int32, string) → int64
        if (funcName == "tcpSend") {
            std::vector<llvm::Value*> args;
            if (auto* argList = ctx->argList())
                for (auto* e : argList->expression())
                    args.push_back(castValue(visit(e)));
            if (!requireArgs(funcName, args, 2)) return {};
            auto* fd = builder_->CreateIntCast(args[0], i32Ty, true);
            std::vector<llvm::Value*> callArgs;
            callArgs.push_back(fd);
            callArgs.push_back(builder_->CreateExtractValue(args[1], 0));
            callArgs.push_back(builder_->CreateExtractValue(args[1], 1));
            auto callee = declareBuiltin("lucis_tcpSend", i64Ty,
                {i32Ty, ptrTy, usizeTy});
            builder_->CreateCall(callee, callArgs);
            return {};
        }
        // tcpRecv/udpRecvFrom: (int32, usize) → string
        if (funcName == "tcpRecv" || funcName == "udpRecvFrom") {
            std::vector<llvm::Value*> args;
            if (auto* argList = ctx->argList())
                for (auto* e : argList->expression())
                    args.push_back(castValue(visit(e)));
            if (!requireArgs(funcName, args, 2)) return {};
            auto* fd = builder_->CreateIntCast(args[0], i32Ty, true);
            auto* maxLen = args[1];
            if (maxLen->getType() != usizeTy)
                maxLen = builder_->CreateIntCast(maxLen, usizeTy, false);
            std::string cName = "lucis_" + funcName;
            auto callee = declareBuiltin(cName, strTy, {i32Ty, usizeTy});
            builder_->CreateCall(callee, {fd, maxLen});
            return {};
        }
        // udpSendTo: (int32, string, string, uint16) → int64
        if (funcName == "udpSendTo") {
            std::vector<llvm::Value*> args;
            if (auto* argList = ctx->argList())
                for (auto* e : argList->expression())
                    args.push_back(castValue(visit(e)));
            if (!requireArgs(funcName, args, 4)) return {};
            auto* fd = builder_->CreateIntCast(args[0], i32Ty, true);
            std::vector<llvm::Value*> callArgs;
            callArgs.push_back(fd);
            callArgs.push_back(builder_->CreateExtractValue(args[1], 0));
            callArgs.push_back(builder_->CreateExtractValue(args[1], 1));
            callArgs.push_back(builder_->CreateExtractValue(args[2], 0));
            callArgs.push_back(builder_->CreateExtractValue(args[2], 1));
            auto* port = builder_->CreateIntCast(args[3], i16Ty, false);
            callArgs.push_back(port);
            auto callee = declareBuiltin("lucis_udpSendTo", i64Ty,
                {i32Ty, ptrTy, usizeTy, ptrTy, usizeTy, i16Ty});
            builder_->CreateCall(callee, callArgs);
            return {};
        }
        // resolve: (string) → string
        if (funcName == "resolve") {
            std::vector<llvm::Value*> args;
            if (auto* argList = ctx->argList())
                for (auto* e : argList->expression())
                    args.push_back(castValue(visit(e)));
            if (!requireArgs(funcName, args, 1)) return {};
            std::vector<llvm::Value*> callArgs;
            callArgs.push_back(builder_->CreateExtractValue(args[0], 0));
            callArgs.push_back(builder_->CreateExtractValue(args[0], 1));
            auto callee = declareBuiltin("lucis_netResolve", strTy,
                {ptrTy, usizeTy});
            builder_->CreateCall(callee, callArgs);
            return {};
        }
    }

    // ── std::thread — yield: void ───────────────────────────────────────
    if (funcName == "yield") {
        auto callee = declareBuiltin("lucis_yield",
            llvm::Type::getVoidTy(*context_), {});
        builder_->CreateCall(callee, {});
        return {};
    }

    // ── std::fs — writeBytes: (string, Vec<uint8>) → void ───────────────
    if (funcName == "writeBytes") {
        std::vector<llvm::Value*> wbArgs;
        if (auto* argList = ctx->argList())
            for (auto* e : argList->expression())
                wbArgs.push_back(castValue(visit(e)));
        if (!requireArgs(funcName, wbArgs, 2)) return {};
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* voidTy  = llvm::Type::getVoidTy(*context_);
        auto* vecTy   = getOrCreateVecStructType();
        auto* pathPtr = builder_->CreateExtractValue(wbArgs[0], 0, "wb_path_ptr");
        auto* pathLen = builder_->CreateExtractValue(wbArgs[0], 1, "wb_path_len");
        auto* vecArgAlloca = builder_->CreateAlloca(vecTy, nullptr, "wb_vec_arg");
        builder_->CreateStore(wbArgs[1], vecArgAlloca);
        auto callee = declareBuiltin("lucis_writeFileBytes", voidTy,
            {ptrTy, usizeTy, ptrTy});
        builder_->CreateCall(callee, {pathPtr, pathLen, vecArgAlloca});
        return {};
    }

    // Collect argument values and their AST expressions
    std::vector<llvm::Value*> args;
    std::vector<LucisParser::ExpressionContext*> argExprs;
    if (auto* argList = ctx->argList()) {
        argExprs = argList->expression();
        for (auto* exprCtx : argExprs) {
            auto val = visit(exprCtx);
            args.push_back(castValue(val));
        }
    }

    for (size_t ai = 0; ai < args.size(); ai++) {
        auto* arg     = args[ai];
        auto* argType = arg->getType();
        if (ai < argExprs.size() && resolveExprArrayDims(argExprs[ai]) > 0) {
            std::cerr << "lucis: function '" << funcName
                      << "' does not accept array arguments directly; "
                         "use '.toString()' or '.join(...)' before printing\n";
            continue;
        }

        // Detect type info via variable lookup
        const TypeInfo* argTypeInfo = nullptr;
        if (auto* load = llvm::dyn_cast<llvm::LoadInst>(arg)) {
            auto* src = load->getPointerOperand();
            for (auto& [vname, info] : locals_) {
                if (info.alloca == src) { argTypeInfo = info.typeInfo; break; }
            }

            // Struct field lookup: LoadInst from GEP into a struct alloca
            if (!argTypeInfo) {
                if (auto* gep = llvm::dyn_cast<llvm::GetElementPtrInst>(src)) {
                    auto* base = gep->getPointerOperand();
                    for (auto& [vname, info] : locals_) {
                        if (info.alloca == base &&
                            info.typeInfo && info.typeInfo->kind == TypeKind::Struct) {
                            if (gep->getNumIndices() == 2) {
                                if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(
                                        gep->getOperand(2))) {
                                    unsigned idx = ci->getZExtValue();
                                    if (idx < info.typeInfo->fields.size())
                                        argTypeInfo = info.typeInfo->fields[idx].typeInfo;
                                }
                            }
                            break;
                        }
                    }
                }
            }

            // Union field access: load directly from union alloca with a
            // different type (no GEP).  The variable lookup above finds the
            // union's own TypeInfo whose builtinSuffix is empty — drop it so
            // the LLVM-type fallback determines the correct suffix.
            if (argTypeInfo && argTypeInfo->kind == TypeKind::Union)
                argTypeInfo = nullptr;
        }

        // String type: extract ptr and len, pass both to builtin
        bool isString = (argTypeInfo && argTypeInfo->kind == TypeKind::String)
                     || (!argTypeInfo && argType->isStructTy());

        if (isString) {
            auto cFuncName = imports_.resolve(funcName, "str");
            if (cFuncName.empty()) {
                std::cerr << "lucis: no builtin '" << funcName
                          << "' for type 'str'\n";
                continue;
            }
            auto* strPtr = builder_->CreateExtractValue(arg, 0, "str_ptr");
            auto* strLen = builder_->CreateExtractValue(arg, 1, "str_len");
            auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto callee   = declareBuiltin(cFuncName,
                                           llvm::Type::getVoidTy(*context_),
                                           { ptrTy, usizeTy });
            builder_->CreateCall(callee, { strPtr, strLen });
            if (ai < argExprs.size()) {
                auto* argExpr = argExprs[ai];
                auto* argExprTI = resolveExprTypeInfo(argExpr);
                auto argExprDims = resolveExprArrayDims(argExpr);
                if (argExprTI && argExprTI->kind == TypeKind::String &&
                    argExprDims == 0 &&
                    !dynamic_cast<LucisParser::IdentExprContext*>(argExpr) &&
                    !isBorrowedStringExpr(argExpr)) {
                    auto freeCallee = declareBuiltin("lucis_freeStr",
                                                     llvm::Type::getVoidTy(*context_),
                                                     {ptrTy, usizeTy});
                    builder_->CreateCall(freeCallee, {strPtr, strLen});
                }
            }
            continue;
        }

        std::string suffix;
        bool isCharLit = (ai < argExprs.size()) &&
            dynamic_cast<LucisParser::CharLitExprContext*>(argExprs[ai]) != nullptr;

        // Try AST-based type resolution for char method calls (e.g. letter.toLower())
        bool isCharExpr = false;
        if (!argTypeInfo && !isCharLit && argType->isIntegerTy(8) && ai < argExprs.size()) {
            auto* exprTI = resolveExprTypeInfo(argExprs[ai]);
            if (exprTI && exprTI->builtinSuffix == "char")
                isCharExpr = true;
        }

        if (argTypeInfo) {
            suffix = argTypeInfo->builtinSuffix;
        } else if (isCharLit || isCharExpr)   { suffix = "char"; }
        else if (argType->isIntegerTy(1))     { suffix = "bool"; }
        else if (argType->isIntegerTy(8))     { suffix = "i8"; }
        else if (argType->isIntegerTy(16))    { suffix = "i16"; }
        else if (argType->isIntegerTy(32))    { suffix = "i32"; }
        else if (argType->isIntegerTy(64))    { suffix = "i64"; }
        else if (argType->isIntegerTy(128))   { suffix = "i128"; }
        else if (argType->isFloatTy())        { suffix = "f32"; }
        else if (argType->isDoubleTy())       { suffix = "f64"; }
        else                                  { suffix = "i32"; }
        auto  cFuncName = imports_.resolve(funcName, suffix);

        if (cFuncName.empty()) {
            std::cerr << "lucis: no builtin '" << funcName
                      << "' for type '" << suffix << "'\n";
            continue;
        }

        // If the actual type is wider than what the C builtin expects (e.g.
        // intinf/i256 → i128), truncate the value before the call.
        llvm::Value* callArg = arg;
        llvm::Type*  paramType = argType;
        if (argType->isIntegerTy(256)) {
            // Untyped integer literal: match suffix to determine target width
            if (suffix == "i128")
                paramType = llvm::Type::getInt128Ty(*context_);
            else if (suffix == "i64")
                paramType = llvm::Type::getInt64Ty(*context_);
            else
                paramType = llvm::Type::getInt32Ty(*context_);
            callArg = builder_->CreateTrunc(arg, paramType);
        }

        auto callee = declareBuiltin(cFuncName,
                                     llvm::Type::getVoidTy(*context_),
                                     { paramType });
        builder_->CreateCall(callee, { callArg });

    }

    return {};
}



std::any IRGen::visitLabelDef(LucisParser::LabelDefContext* ctx) {
        SET_DBG_LOC(ctx);
    auto name = ctx->IDENTIFIER()->getText();
    auto* bb = labels_[name];
    if (!bb) {
        bb = llvm::BasicBlock::Create(*context_, "label." + name, currentFunction_);
        labels_[name] = bb;
    }
    if (!builder_->GetInsertBlock()->getTerminator())
        builder_->CreateBr(bb);
    builder_->SetInsertPoint(bb);
    visit(ctx->statement());
    return {};
}



std::any IRGen::visitAsmStmt(LucisParser::AsmStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto isVolatile = ctx->VOLATILE() != nullptr;

    // Build asm string from one or more string literals (concatenated with newlines)
    std::string asmStr;
    for (auto* strTok : ctx->STR_LIT()) {
        auto raw = strTok->getText();
        auto content = raw.substr(1, raw.size() - 2);
        if (!asmStr.empty()) asmStr += '\n';
        asmStr += content;
    }

    // Translate GCC %= to LLVM ${:uid} for unique labels per asm instance
    for (size_t p = 0; (p = asmStr.find("%=", p)) != std::string::npos; p += 7)
        asmStr.replace(p, 2, "${:uid}");

    auto& context = *context_;
    auto* voidTy  = llvm::Type::getVoidTy(context);

    std::vector<llvm::Type*>  paramTypes;
    std::vector<llvm::Value*> paramValues;
    std::vector<std::string>  outConstraints;
    std::vector<const TypeInfo*> outTypeInfos;
    std::vector<llvm::AllocaInst*> outAllocas;

    std::string constraintsStr;

    auto append = [&](const std::string& c) {
        if (!constraintsStr.empty()) constraintsStr += ',';
        constraintsStr += c;
    };

    // Outputs: "=r"(var) or "+r"(var)
    if (auto* outList = ctx->asmOutputList()) {
        size_t outIdx = 0;
        for (auto* out : outList->asmOutput()) {
            auto raw = out->constraint->getText();
            auto constraint = raw.substr(1, raw.size() - 2);
            auto* ident = out->IDENTIFIER();
            if (!ident) continue;
            auto varName = ident->getText();
            auto it = locals_.find(varName);
            if (it == locals_.end()) return {};
            outConstraints.push_back(constraint);
            outTypeInfos.push_back(it->second.typeInfo);
            outAllocas.push_back(it->second.alloca);

            if (!constraint.empty() && constraint[0] == '+') {
                // "+r" → expand to "=r" output + matching "N" input
                auto core = constraint.substr(1);
                append('=' + core);
                auto matchIdx = std::to_string(outIdx);
                append(matchIdx);
                auto* llvmTy = it->second.typeInfo->toLLVMType(context, module_->getDataLayout());
                auto* loaded = builder_->CreateLoad(llvmTy, it->second.alloca, varName + ".in");
                paramTypes.push_back(loaded->getType());
                paramValues.push_back(loaded);
                outConstraints.back() = '=' + core;
            } else {
                append(constraint);
            }
            outIdx++;
        }
    }

    // Inputs: "r"(expr) or "m"(expr)
    if (auto* inList = ctx->asmInputList()) {
        for (auto* operand : inList->asmOperand()) {
            auto raw = operand->constraint->getText();
            auto constraint = raw.substr(1, raw.size() - 2);
            auto* val = castValue(visit(operand->expression()));
            if (!val) return {};
            append(constraint);
            paramTypes.push_back(val->getType());
            paramValues.push_back(val);
        }
    }

    // Goto labels (asm goto) — MUST come before clobbers in constraint string
    // Uses LLVM 15+ "!i" constraint (blockaddress not passed as arg)
    auto isGoto = ctx->GOTO() != nullptr && ctx->asmGotoLabelList()
                  && !ctx->asmGotoLabelList()->IDENTIFIER().empty();
    std::vector<llvm::BasicBlock*> indirectDests;
    if (isGoto) {
        for (auto* ident : ctx->asmGotoLabelList()->IDENTIFIER()) {
            auto name = ident->getText();
            auto* labelBB = labels_[name];
            if (!labelBB) {
                labelBB = llvm::BasicBlock::Create(*context_, "label." + name, currentFunction_);
                labels_[name] = labelBB;
            }
            indirectDests.push_back(labelBB);
            append("!i");
        }
    }

    // Clobbers: "~{reg}" (must come after all operand constraints, including labels)
    if (auto* clobberList = ctx->asmClobberList()) {
        for (auto* strTok : clobberList->STR_LIT()) {
            auto raw = strTok->getText();
            auto reg = raw.substr(1, raw.size() - 2);
            append("~{" + reg + "}");
        }
    }

    // Determine return type
    llvm::Type* retType = voidTy;
    if (outAllocas.size() == 1) {
        retType = outTypeInfos[0]->toLLVMType(*context_, module_->getDataLayout());
    } else if (outAllocas.size() > 1) {
        std::vector<llvm::Type*> members;
        for (auto* ti : outTypeInfos)
            members.push_back(ti->toLLVMType(*context_, module_->getDataLayout()));
        retType = llvm::StructType::get(*context_, members);
    }

    auto* ft = llvm::FunctionType::get(retType, paramTypes, false);
    auto* asmFn = llvm::InlineAsm::get(ft, asmStr, constraintsStr,
                                        isVolatile || isGoto, false,
                                        ctx->INTEL() ? llvm::InlineAsm::AD_Intel
                                                     : llvm::InlineAsm::AD_ATT);

    if (isGoto) {
        auto* fallthroughBB = llvm::BasicBlock::Create(*context_, "asm.fallthrough", currentFunction_);
        auto* result = builder_->CreateCallBr(ft, asmFn, fallthroughBB, indirectDests, paramValues);

        builder_->SetInsertPoint(fallthroughBB);
        if (outAllocas.size() == 1 && outAllocas[0]) {
            builder_->CreateStore(result, outAllocas[0]);
        } else if (outAllocas.size() > 1) {
            for (size_t i = 0; i < outAllocas.size(); i++) {
                if (outAllocas[i]) {
                    auto* val = builder_->CreateExtractValue(result, i, "out" + std::to_string(i));
                    builder_->CreateStore(val, outAllocas[i]);
                }
            }
        }
    } else {
        auto* result = builder_->CreateCall(ft, asmFn, paramValues);

        if (outAllocas.size() == 1 && outAllocas[0]) {
            builder_->CreateStore(result, outAllocas[0]);
        } else if (outAllocas.size() > 1) {
            for (size_t i = 0; i < outAllocas.size(); i++) {
                if (outAllocas[i]) {
                    auto* val = builder_->CreateExtractValue(result, i, "out" + std::to_string(i));
                    builder_->CreateStore(val, outAllocas[i]);
                }
            }
        }
    }

    return {};
}



std::any IRGen::visitAsmExpr(LucisParser::AsmExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto isVolatile = ctx->VOLATILE() != nullptr;

    // Build asm string from one or more string literals (concatenated with newlines)
    std::string asmStr;
    for (auto* strTok : ctx->STR_LIT()) {
        auto raw = strTok->getText();
        auto content = raw.substr(1, raw.size() - 2);
        if (!asmStr.empty()) asmStr += '\n';
        asmStr += content;
    }

    // Translate GCC %= to LLVM ${:uid} for unique labels per asm instance
    for (size_t p = 0; (p = asmStr.find("%=", p)) != std::string::npos; p += 7)
        asmStr.replace(p, 2, "${:uid}");

    auto& context = *context_;
    auto* voidTy  = llvm::Type::getVoidTy(context);

    std::vector<llvm::Type*>  paramTypes;
    std::vector<llvm::Value*> paramValues;
    std::vector<const TypeInfo*> outTypeInfos;
    std::vector<llvm::AllocaInst*> outAllocas;
    std::string constraintsStr;

    auto append = [&](const std::string& c) {
        if (!constraintsStr.empty()) constraintsStr += ',';
        constraintsStr += c;
    };

    if (auto* outList = ctx->asmOutputList()) {
        size_t outIdx = 0;
        for (auto* out : outList->asmOutput()) {
            auto raw = out->constraint->getText();
            auto constraint = raw.substr(1, raw.size() - 2);
            auto* ident = out->IDENTIFIER();

            if (ident) {
                // Named output: "=r"(var)
                auto varName = ident->getText();
                auto it = locals_.find(varName);
                if (it == locals_.end()) return std::any(llvm::UndefValue::get(voidTy));
                outTypeInfos.push_back(it->second.typeInfo);
                outAllocas.push_back(it->second.alloca);

                if (!constraint.empty() && constraint[0] == '+') {
                    auto core = constraint.substr(1);
                    append('=' + core);
                    append(std::to_string(outIdx));
                    auto* loaded = builder_->CreateLoad(
                        it->second.typeInfo->toLLVMType(context, module_->getDataLayout()),
                        it->second.alloca, varName + ".in");
                    paramTypes.push_back(loaded->getType());
                    paramValues.push_back(loaded);
                } else {
                    append(constraint);
                }
            } else {
                // Unnamed output: "=r" — type defaults to int64
                append(constraint);
                auto* defaultTy = typeRegistry_.lookup("int64");
                outTypeInfos.push_back(defaultTy);
                outAllocas.push_back(nullptr);
            }
            outIdx++;
        }
    }

    if (auto* inList = ctx->asmInputList()) {
        for (auto* operand : inList->asmOperand()) {
            auto raw = operand->constraint->getText();
            auto constraint = raw.substr(1, raw.size() - 2);
            auto* val = castValue(visit(operand->expression()));
            if (!val) return llvm::UndefValue::get(voidTy);
            append(constraint);
            paramTypes.push_back(val->getType());
            paramValues.push_back(val);
        }
    }

    if (auto* clobberList = ctx->asmClobberList()) {
        for (auto* strTok : clobberList->STR_LIT()) {
            auto raw = strTok->getText();
            auto reg = raw.substr(1, raw.size() - 2);
            append("~{" + reg + "}");
        }
    }

    llvm::Type* retType = voidTy;
    if (outAllocas.size() == 1) {
        retType = outTypeInfos[0]->toLLVMType(context, module_->getDataLayout());
    } else if (outAllocas.size() > 1) {
        std::vector<llvm::Type*> members;
        for (auto* ti : outTypeInfos)
            members.push_back(ti->toLLVMType(context, module_->getDataLayout()));
        retType = llvm::StructType::get(context, members);
    }

    auto* ft = llvm::FunctionType::get(retType, paramTypes, false);
    auto* asmFn = llvm::InlineAsm::get(ft, asmStr, constraintsStr,
                                        isVolatile, false,
                                        ctx->INTEL() ? llvm::InlineAsm::AD_Intel
                                                     : llvm::InlineAsm::AD_ATT);
    auto* result = builder_->CreateCall(ft, asmFn, paramValues);

    if (outAllocas.size() == 1 && outAllocas[0]) {
        builder_->CreateStore(result, outAllocas[0]);
    } else if (outAllocas.size() > 1) {
        for (size_t i = 0; i < outAllocas.size(); i++) {
            if (outAllocas[i]) {
                auto* val = builder_->CreateExtractValue(result, i, "out" + std::to_string(i));
                builder_->CreateStore(val, outAllocas[i]);
            }
        }
    }

    return result ? std::any(static_cast<llvm::Value*>(result)) : std::any(llvm::UndefValue::get(voidTy));
}



std::any IRGen::visitReturnStmt(LucisParser::ReturnStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    // Handle bare `ret;` in void functions
    if (!ctx->expression()) {
        emitAllCleanups();
        builder_->CreateRetVoid();
        return {};
    }

    auto  val    = visit(ctx->expression());
    auto* retVal = castValue(val);

    // Fallible return: if returning from !T function, auto-wrap
    if (currentFnReturnType_ && currentFnReturnType_->kind == TypeKind::Fallible) {
        auto* fnTy = currentFnReturnType_;
        auto* fnLLTy = fnTy->toLLVMType(*context_, module_->getDataLayout());
        auto* valTy = retVal->getType();
        auto* succTy = fnTy->successType->toLLVMType(*context_, module_->getDataLayout());
        auto* errTy = fnTy->errorType->toLLVMType(*context_, module_->getDataLayout());

        // Check if expression is an error type → tag=1, else tag=0
        bool isError = (valTy == errTy);
        auto* tag = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), isError ? 1 : 0);

        auto* alloca = builder_->CreateAlloca(fnLLTy, nullptr, "ret.fallible");
        builder_->CreateStore(tag,
            builder_->CreateStructGEP(fnLLTy, alloca, 0));
        builder_->CreateStore(retVal,
            builder_->CreateStructGEP(fnLLTy, alloca, isError ? 2 : 1));
        retVal = builder_->CreateLoad(fnLLTy, alloca);
        // Fall through to cleanup + ret
    }

    // Cast to the function's return type if needed
    auto* expectedTy = builder_->getCurrentFunctionReturnType();
    if (retVal->getType() != expectedTy) {
        if (retVal->getType()->isIntegerTy() && expectedTy->isIntegerTy()) {
            retVal = builder_->CreateIntCast(retVal, expectedTy, true, "ret_cast");
        } else if (retVal->getType()->isFloatingPointTy() && expectedTy->isFloatingPointTy()) {
            if (retVal->getType()->getPrimitiveSizeInBits() > expectedTy->getPrimitiveSizeInBits())
                retVal = builder_->CreateFPTrunc(retVal, expectedTy, "ret_trunc");
            else
                retVal = builder_->CreateFPExt(retVal, expectedTy, "ret_ext");
        } else if (retVal->getType()->isStructTy() && expectedTy->isStructTy()) {
            // Tuple/struct return: repack element-by-element with casts
            auto* srcST = llvm::cast<llvm::StructType>(retVal->getType());
            auto* dstST = llvm::cast<llvm::StructType>(expectedTy);
            if (srcST->getNumElements() == dstST->getNumElements()) {
                llvm::Value* agg = llvm::UndefValue::get(dstST);
                for (unsigned i = 0; i < srcST->getNumElements(); i++) {
                    auto* elem = builder_->CreateExtractValue(retVal, {i});
                    auto* dstElemTy = dstST->getElementType(i);
                    if (elem->getType() != dstElemTy) {
                        if (elem->getType()->isIntegerTy() && dstElemTy->isIntegerTy())
                            elem = builder_->CreateIntCast(elem, dstElemTy, true);
                        else if (elem->getType()->isFloatingPointTy() && dstElemTy->isFloatingPointTy()) {
                            if (elem->getType()->getPrimitiveSizeInBits() > dstElemTy->getPrimitiveSizeInBits())
                                elem = builder_->CreateFPTrunc(elem, dstElemTy);
                            else
                                elem = builder_->CreateFPExt(elem, dstElemTy);
                        }
                    }
                    agg = builder_->CreateInsertValue(agg, elem, {i});
                }
                retVal = agg;
            }
        }
    }

    // Emit deferred and auto cleanups before returning.
    // If returning an owned value, consume the source so auto-cleanup
    // doesn't free it (skipVar handles bare local names; consume handles
    // field accesses like `tok.lexeme`).
    std::string skipVar;
    if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(ctx->expression())) {
        skipVar = ident->IDENTIFIER()->getText();
    } else {
        consumeExprIfOwnedLocal(ctx->expression());
    }
    emitAllCleanups(skipVar);

    builder_->CreateRet(retVal);
    return {};
}



std::any IRGen::visitIfStmt(LucisParser::IfStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto elseIfs = ctx->elseIfClause();
    auto* elseClause = ctx->elseClause();

    auto emitIfBody = [&](LucisParser::IfBodyContext* body) {
        if (!body) return;
        auto savedLocals = locals_;
        size_t savedDeferBase = deferStack_.size();
        if (auto* b = body->block()) {
            for (auto* stmt : b->statement())
                visit(stmt);
        } else if (auto* s = body->statement()) {
            visit(s);
        }
        // On normal block exit, emit deferred cleanups registered in this scope
        // (scope-guard semantics for `defer` inside conditional branches).
        auto* bb = builder_->GetInsertBlock();
        if (bb && !bb->getTerminator()) {
            for (size_t i = deferStack_.size(); i-- > savedDeferBase;)
                emitOneDeferred(deferStack_[i]);
        }
        deferStack_.resize(savedDeferBase);
        emitBlockExitCleanups(savedLocals);
        locals_ = savedLocals;
    };

    // ── Dead branch elimination for @cfg(…) conditions ─────────
    {
        auto* condExpr = ctx->expression();
        if (auto* cfgCtx = dynamic_cast<LucisParser::CfgExprContext*>(condExpr)) {
            auto* inner = cfgCtx->expression();
            bool cfgVal = evalCfgPredicate(inner);
            if (cfgVal) {
                // @cfg(true): emit only the if-body, skip else/elif
                emitIfBody(ctx->ifBody());
                // If body didn't terminate, need a merge block
                if (!builder_->GetInsertBlock()->getTerminator()) {
                    auto* mergeBlock = llvm::BasicBlock::Create(*context_, "if.end", currentFunction_);
                    builder_->CreateBr(mergeBlock);
                    builder_->SetInsertPoint(mergeBlock);
                }
                return std::any();
            } else {
                // @cfg(false): skip if-body, find the first non-false elif or else
                bool foundBranch = false;
                for (auto* elseIf : elseIfs) {
                    auto* eifCfg = dynamic_cast<LucisParser::CfgExprContext*>(elseIf->expression());
                    if (eifCfg) {
                        bool eifVal = evalCfgPredicate(eifCfg->expression());
                        if (eifVal) {
                            emitIfBody(elseIf->ifBody());
                            foundBranch = true;
                            break;
                        }
                    } else {
                        // Non-cfg elif — evaluate at runtime
                        auto* cond = castValue(visit(elseIf->expression()));
                        cond = toBool(cond, resolveExprTypeInfo(elseIf->expression()), "tobool");
                        auto* bodyBlock = llvm::BasicBlock::Create(*context_, "elif.then", currentFunction_);
                        auto* mergeBlock = llvm::BasicBlock::Create(*context_, "if.end", currentFunction_);
                        builder_->CreateCondBr(cond, bodyBlock, mergeBlock);
                        builder_->SetInsertPoint(bodyBlock);
                        emitIfBody(elseIf->ifBody());
                        if (!builder_->GetInsertBlock()->getTerminator())
                            builder_->CreateBr(mergeBlock);
                        builder_->SetInsertPoint(mergeBlock);
                        foundBranch = true;
                        break;
                    }
                }
                if (!foundBranch && elseClause) {
                    emitIfBody(elseClause->ifBody());
                }
                if (!builder_->GetInsertBlock()->getTerminator()) {
                    auto* mergeBlock = llvm::BasicBlock::Create(*context_, "if.end", currentFunction_);
                    builder_->CreateBr(mergeBlock);
                    builder_->SetInsertPoint(mergeBlock);
                }
                return std::any();
            }
        }
    }

    // Total branches: 1 (if) + N (else if) + optional else
    size_t totalBranches = 1 + elseIfs.size();

    // Create all basic blocks upfront
    std::vector<llvm::BasicBlock*> condBlocks;   // condition eval blocks
    std::vector<llvm::BasicBlock*> bodyBlocks;    // body blocks
    auto* mergeBlock = llvm::BasicBlock::Create(*context_, "if.end", currentFunction_);

    // First body block (the if-body)
    bodyBlocks.push_back(
        llvm::BasicBlock::Create(*context_, "if.then", currentFunction_));

    // Else-if condition + body blocks
    for (size_t i = 0; i < elseIfs.size(); i++) {
        condBlocks.push_back(
            llvm::BasicBlock::Create(*context_, "elif.cond", currentFunction_));
        bodyBlocks.push_back(
            llvm::BasicBlock::Create(*context_, "elif.then", currentFunction_));
    }

    // Else body block
    llvm::BasicBlock* elseBlock = nullptr;
    if (elseClause) {
        elseBlock = llvm::BasicBlock::Create(*context_, "if.else", currentFunction_);
    }

    // ── Emit the if-condition ──────────────────────────────────────
    auto* cond = castValue(visit(ctx->expression()));
    cond = toBool(cond, resolveExprTypeInfo(ctx->expression()), "tobool");

    auto* falseTarget = !condBlocks.empty() ? condBlocks[0]
                      : elseBlock           ? elseBlock
                                            : mergeBlock;
    builder_->CreateCondBr(cond, bodyBlocks[0], falseTarget);

    // ── Emit the if-body ───────────────────────────────────────────
    builder_->SetInsertPoint(bodyBlocks[0]);
    {
        // Consume any pending is-binding from the condition expression.
        auto isBinding = std::exchange(pendingIsBinding_, std::nullopt);
        if (isBinding && isBinding->typeInfo && isBinding->llvmType) {
            auto* bindAlloca = builder_->CreateAlloca(isBinding->llvmType, nullptr, isBinding->name);
            auto* payloadVal = builder_->CreateLoad(isBinding->llvmType, isBinding->payloadPtr, isBinding->name + ".val");
            builder_->CreateStore(payloadVal, bindAlloca);
            locals_[isBinding->name] = {bindAlloca, isBinding->typeInfo, 0};
        }
        emitIfBody(ctx->ifBody());
        if (isBinding) locals_.erase(isBinding->name);
    }
    if (!builder_->GetInsertBlock()->getTerminator())
        builder_->CreateBr(mergeBlock);

    // ── Emit else-if chains ────────────────────────────────────────
    for (size_t i = 0; i < elseIfs.size(); i++) {
        // Condition
        builder_->SetInsertPoint(condBlocks[i]);
        auto* eifCond = castValue(visit(elseIfs[i]->expression()));
        eifCond = toBool(eifCond,
            resolveExprTypeInfo(elseIfs[i]->expression()), "tobool");

        auto* nextFalse = (i + 1 < condBlocks.size()) ? condBlocks[i + 1]
                        : elseBlock                    ? elseBlock
                                                       : mergeBlock;
        builder_->CreateCondBr(eifCond, bodyBlocks[i + 1], nextFalse);

        // Body
        builder_->SetInsertPoint(bodyBlocks[i + 1]);
        {
            auto elifBinding = std::exchange(pendingIsBinding_, std::nullopt);
            if (elifBinding && elifBinding->typeInfo && elifBinding->llvmType) {
                auto* bindAlloca = builder_->CreateAlloca(elifBinding->llvmType, nullptr, elifBinding->name);
                auto* payloadVal = builder_->CreateLoad(elifBinding->llvmType, elifBinding->payloadPtr, elifBinding->name + ".val");
                builder_->CreateStore(payloadVal, bindAlloca);
                locals_[elifBinding->name] = {bindAlloca, elifBinding->typeInfo, 0};
            }
            emitIfBody(elseIfs[i]->ifBody());
            if (elifBinding) locals_.erase(elifBinding->name);
        }
        if (!builder_->GetInsertBlock()->getTerminator())
            builder_->CreateBr(mergeBlock);
    }

    // ── Emit else body ─────────────────────────────────────────────
    if (elseBlock) {
        builder_->SetInsertPoint(elseBlock);
        emitIfBody(elseClause->ifBody());
        if (!builder_->GetInsertBlock()->getTerminator())
            builder_->CreateBr(mergeBlock);
    }

    // ── Continue at merge ──────────────────────────────────────────
    builder_->SetInsertPoint(mergeBlock);
    return {};
}



std::any IRGen::visitForClassicStmt(LucisParser::ForClassicStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    // for TYPE NAME = init; cond; update { body }
    auto* typeInfo = [&]() -> const TypeInfo* {
        if (!currentGenericSubst_.empty())
            return resolveTypeInfoWithSubst(ctx->typeSpec(), currentGenericSubst_);
        return resolveTypeInfo(ctx->typeSpec());
    }();
    auto* varType  = typeInfo->toLLVMType(*context_, module_->getDataLayout());
    auto  varName  = ctx->IDENTIFIER()->getText();

    auto exprs = ctx->expression();

    // Init
    auto* initVal = castValue(visit(exprs[0]));
    if (initVal->getType() != varType)
        initVal = builder_->CreateIntCast(initVal, varType, typeInfo->isSigned, "cast");
    auto* alloca = builder_->CreateAlloca(varType, nullptr, varName);
    builder_->CreateStore(initVal, alloca);
    locals_[varName] = {alloca, typeInfo, 0};

    // Basic blocks
    auto* condBB   = llvm::BasicBlock::Create(*context_, "for.cond", currentFunction_);
    auto* bodyBB   = llvm::BasicBlock::Create(*context_, "for.body", currentFunction_);
    auto* updateBB = llvm::BasicBlock::Create(*context_, "for.update", currentFunction_);
    auto* endBB    = llvm::BasicBlock::Create(*context_, "for.end", currentFunction_);

    loopStack_.push_back({endBB, updateBB});

    builder_->CreateBr(condBB);

    // Condition
    builder_->SetInsertPoint(condBB);
    auto* cond = castValue(visit(exprs[1]));
    cond = toBool(cond, resolveExprTypeInfo(exprs[1]), "tobool");
    builder_->CreateCondBr(cond, bodyBB, endBB);

    // Body
    builder_->SetInsertPoint(bodyBB);
    {
        auto savedLocals = locals_;
        loopBodyLocalsStack_.push_back(savedLocals);
        for (auto* stmt : ctx->block()->statement())
            visit(stmt);
        emitBlockExitCleanups(savedLocals);
        loopBodyLocalsStack_.pop_back();
        locals_ = savedLocals;
    }
    if (!builder_->GetInsertBlock()->getTerminator())
        builder_->CreateBr(updateBB);

    // Update
    builder_->SetInsertPoint(updateBB);
    visit(exprs[2]);
    builder_->CreateBr(condBB);

    loopStack_.pop_back();
    locals_.erase(varName);

    builder_->SetInsertPoint(endBB);
    return {};
}



std::any IRGen::visitForInStmt(LucisParser::ForInStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* typeInfo = [&]() -> const TypeInfo* {
        if (!currentGenericSubst_.empty())
            return resolveTypeInfoWithSubst(ctx->typeSpec(), currentGenericSubst_);
        return resolveTypeInfo(ctx->typeSpec());
    }();
    auto* varType  = typeInfo->toLLVMType(*context_, module_->getDataLayout());
    auto  varName  = ctx->IDENTIFIER()->getText();
    auto* iterExpr = ctx->expression();

    // Check if iterating over a range (..  or ..=)
    bool isRange = dynamic_cast<LucisParser::RangeExprContext*>(iterExpr) ||
                   dynamic_cast<LucisParser::RangeInclExprContext*>(iterExpr);
    bool isInclusive = dynamic_cast<LucisParser::RangeInclExprContext*>(iterExpr) != nullptr;

    if (isRange) {
        // Range iteration: for TYPE i in start..end  or  start..=end
        llvm::Value* startVal;
        llvm::Value* endVal;

        if (isInclusive) {
            auto* rng = dynamic_cast<LucisParser::RangeInclExprContext*>(iterExpr);
            startVal = castValue(visit(rng->expression(0)));
            endVal   = castValue(visit(rng->expression(1)));
        } else {
            auto* rng = dynamic_cast<LucisParser::RangeExprContext*>(iterExpr);
            startVal = castValue(visit(rng->expression(0)));
            endVal   = castValue(visit(rng->expression(1)));
        }

        // Cast to target type
        if (startVal->getType() != varType)
            startVal = builder_->CreateIntCast(startVal, varType, typeInfo->isSigned, "cast");
        if (endVal->getType() != varType)
            endVal = builder_->CreateIntCast(endVal, varType, typeInfo->isSigned, "cast");

        auto* alloca = builder_->CreateAlloca(varType, nullptr, varName);
        builder_->CreateStore(startVal, alloca);
        locals_[varName] = {alloca, typeInfo, 0};

        auto* condBB   = llvm::BasicBlock::Create(*context_, "for.cond", currentFunction_);
        auto* bodyBB   = llvm::BasicBlock::Create(*context_, "for.body", currentFunction_);
        auto* updateBB = llvm::BasicBlock::Create(*context_, "for.update", currentFunction_);
        auto* endBB    = llvm::BasicBlock::Create(*context_, "for.end", currentFunction_);

        loopStack_.push_back({endBB, updateBB});

        builder_->CreateBr(condBB);

        // Condition: i < end  (exclusive) or  i <= end (inclusive)
        builder_->SetInsertPoint(condBB);
        auto* cur = builder_->CreateLoad(varType, alloca, varName);
        llvm::Value* cond;
        if (isInclusive)
            cond = typeInfo->isSigned
                 ? builder_->CreateICmpSLE(cur, endVal, "cmp")
                 : builder_->CreateICmpULE(cur, endVal, "cmp");
        else
            cond = typeInfo->isSigned
                 ? builder_->CreateICmpSLT(cur, endVal, "cmp")
                 : builder_->CreateICmpULT(cur, endVal, "cmp");
        builder_->CreateCondBr(cond, bodyBB, endBB);

        // Body
        builder_->SetInsertPoint(bodyBB);
        {
            auto savedLocals = locals_;
            loopBodyLocalsStack_.push_back(savedLocals);
            for (auto* stmt : ctx->block()->statement())
                visit(stmt);
            emitBlockExitCleanups(savedLocals);
            loopBodyLocalsStack_.pop_back();
            locals_ = savedLocals;
        }
        if (!builder_->GetInsertBlock()->getTerminator())
            builder_->CreateBr(updateBB);

        // Update: i++
        builder_->SetInsertPoint(updateBB);
        auto* curVal = builder_->CreateLoad(varType, alloca, "cur");
        auto* next = builder_->CreateAdd(curVal,
            llvm::ConstantInt::get(varType, 1), "inc");
        builder_->CreateStore(next, alloca);
        builder_->CreateBr(condBB);

        loopStack_.pop_back();
        locals_.erase(varName);

        builder_->SetInsertPoint(endBB);
    } else {
        // Check if iterating over a variadic parameter
        std::string iterName;
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(iterExpr))
            iterName = ident->IDENTIFIER()->getText();

        auto vpIt = variadicParams_.find(iterName);
        bool isVariadicParam = !iterName.empty() && vpIt != variadicParams_.end();

        if (isVariadicParam) {
            // Variadic param iteration: for TYPE x in variadicParam
            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
            if (!vpIt->second.elementType) {
                std::cerr << "lucis: internal error: variadic param missing elementType\n";
                return {};
            }
            auto* elemTy = vpIt->second.elementType->toLLVMType(*context_, module_->getDataLayout());

            auto* dataPtr = builder_->CreateLoad(ptrTy, vpIt->second.dataPtr, "var_data");
            auto* varLen  = builder_->CreateLoad(i64Ty, vpIt->second.lenAlloca, "var_len");

            auto* idxAlloca = builder_->CreateAlloca(i64Ty, nullptr, "idx");
            builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), idxAlloca);

            auto* elemAlloca = builder_->CreateAlloca(varType, nullptr, varName);
            locals_[varName] = {elemAlloca, typeInfo, 0};

            auto* condBB   = llvm::BasicBlock::Create(*context_, "for.cond", currentFunction_);
            auto* bodyBB   = llvm::BasicBlock::Create(*context_, "for.body", currentFunction_);
            auto* updateBB = llvm::BasicBlock::Create(*context_, "for.update", currentFunction_);
            auto* endBB    = llvm::BasicBlock::Create(*context_, "for.end", currentFunction_);

            loopStack_.push_back({endBB, updateBB});
            builder_->CreateBr(condBB);

            // Condition: idx < len
            builder_->SetInsertPoint(condBB);
            auto* idx  = builder_->CreateLoad(i64Ty, idxAlloca, "idx");
            auto* cond = builder_->CreateICmpULT(idx, varLen, "cmp");
            builder_->CreateCondBr(cond, bodyBB, endBB);

            // Body: load element via GEP on data pointer
            builder_->SetInsertPoint(bodyBB);
            auto* curIdx = builder_->CreateLoad(i64Ty, idxAlloca, "idx");
            auto* gep = builder_->CreateGEP(elemTy, dataPtr, curIdx, "elem_ptr");
            auto* elem = builder_->CreateLoad(varType, gep, "elem");
            builder_->CreateStore(elem, elemAlloca);

            {
                auto savedLocals = locals_;
                loopBodyLocalsStack_.push_back(savedLocals);
                for (auto* stmt : ctx->block()->statement())
                    visit(stmt);
                emitBlockExitCleanups(savedLocals);
                loopBodyLocalsStack_.pop_back();
                locals_ = savedLocals;
            }
            if (!builder_->GetInsertBlock()->getTerminator())
                builder_->CreateBr(updateBB);

            // Update: idx++
            builder_->SetInsertPoint(updateBB);
            auto* curIdxUpd = builder_->CreateLoad(i64Ty, idxAlloca, "idx");
            auto* nextIdx   = builder_->CreateAdd(curIdxUpd,
                llvm::ConstantInt::get(i64Ty, 1), "inc");
            builder_->CreateStore(nextIdx, idxAlloca);
            builder_->CreateBr(condBB);

            loopStack_.pop_back();
            locals_.erase(varName);

            builder_->SetInsertPoint(endBB);
        } else {
        // Detect if the iterable is a Vec<T> (Extended type)
        const TypeInfo* iterableTI = nullptr;
        llvm::AllocaInst* vecAlloca = nullptr;
        unsigned iterableDims = 0;
        bool isTempVec = false;
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(iterExpr)) {
            auto it = locals_.find(ident->IDENTIFIER()->getText());
            if (it != locals_.end()) {
                iterableTI = it->second.typeInfo;
                vecAlloca  = it->second.alloca;
                iterableDims = it->second.arrayDims;
            }
        }

        // Field access on a struct variable (foo.bar): use GEP into the struct directly
        // so we don't create a shallow-copy temp that gets freed after the loop,
        // which would cause a double-free when the struct is also dropped.
        if (!vecAlloca) {
            if (auto* fieldAccess = dynamic_cast<LucisParser::FieldAccessExprContext*>(iterExpr)) {
                if (auto* baseIdent = dynamic_cast<LucisParser::IdentExprContext*>(
                        fieldAccess->expression())) {
                    auto baseName = baseIdent->IDENTIFIER()->getText();
                    auto it = locals_.find(baseName);
                    if (it != locals_.end() && it->second.typeInfo &&
                        (it->second.typeInfo->kind == TypeKind::Struct ||
                         (it->second.typeInfo->kind == TypeKind::Pointer &&
                          it->second.typeInfo->pointeeType &&
                          it->second.typeInfo->pointeeType->kind == TypeKind::Struct))) {
                        auto* structTI = (it->second.typeInfo->kind == TypeKind::Pointer)
                            ? it->second.typeInfo->pointeeType
                            : it->second.typeInfo;
                        auto fieldName = fieldAccess->IDENTIFIER()->getText();
                        for (size_t i = 0; i < structTI->fields.size(); i++) {
                            if (structTI->fields[i].name == fieldName) {
                                auto* structLLTy = llvm::cast<llvm::StructType>(
                                    structTI->toLLVMType(*context_, module_->getDataLayout()));
                                llvm::Value* basePtr = it->second.alloca;
                                if (it->second.typeInfo->kind == TypeKind::Pointer) {
                                    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                                    basePtr = builder_->CreateLoad(ptrTy, basePtr,
                                        baseName + "_selfptr");
                                }
                                auto* gep = builder_->CreateStructGEP(
                                    structLLTy, basePtr,
                                    static_cast<unsigned>(i), fieldName + "_iter");
                                vecAlloca = reinterpret_cast<llvm::AllocaInst*>(gep);
                                iterableTI = structTI->fields[i].typeInfo;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Support any expression that returns Vec (method calls, function calls, etc.)
        if (!vecAlloca) {
            auto* exprVal = castValue(visit(iterExpr));
            auto* exprTI  = resolveExprTypeInfo(iterExpr);
            if (exprTI && exprTI->kind == TypeKind::Extended) {
                auto* vecTy  = getOrCreateVecStructType();
                vecAlloca = builder_->CreateAlloca(vecTy, nullptr, "iter_tmp");
                builder_->CreateStore(exprVal, vecAlloca);
                iterableTI = exprTI;
                isTempVec = true;
            }
        }

        bool isVec = iterableTI && iterableTI->kind == TypeKind::Extended;

        if (isVec) {
            // Vec iteration: for TYPE x in vec
            // Determine suffix: from Vec's element type, or from the loop variable type
            std::string suffix;
            if (iterableTI->elementType)
                suffix = getVecSuffix(iterableTI->elementType);
            else
                suffix = typeInfo->builtinSuffix.empty() ? "raw" : typeInfo->builtinSuffix;
            auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);

            // Get vec length: lucis_vec_len_<suffix>(&vec)
            auto lenCallee = declareBuiltin("lucis_vec_len_" + suffix, usizeTy, {ptrTy});
            auto* vecLen = builder_->CreateCall(lenCallee, {vecAlloca}, "vec_len");

            auto* idxType   = usizeTy;
            auto* idxAlloca = builder_->CreateAlloca(idxType, nullptr, "idx");
            builder_->CreateStore(llvm::ConstantInt::get(idxType, 0), idxAlloca);

            auto* elemAlloca = builder_->CreateAlloca(varType, nullptr, varName);
            locals_[varName] = {elemAlloca, typeInfo, 0};

            auto* condBB   = llvm::BasicBlock::Create(*context_, "for.cond", currentFunction_);
            auto* bodyBB   = llvm::BasicBlock::Create(*context_, "for.body", currentFunction_);
            auto* updateBB = llvm::BasicBlock::Create(*context_, "for.update", currentFunction_);
            auto* endBB    = llvm::BasicBlock::Create(*context_, "for.end", currentFunction_);

            loopStack_.push_back({endBB, updateBB});

            builder_->CreateBr(condBB);

            // Condition: idx < vec.len()
            builder_->SetInsertPoint(condBB);
            auto* idx  = builder_->CreateLoad(idxType, idxAlloca, "idx");
            auto* cond = builder_->CreateICmpULT(idx, vecLen, "cmp");
            builder_->CreateCondBr(cond, bodyBB, endBB);

            // Body: elem = lucis_vec_at_<suffix>(&vec, idx)
            builder_->SetInsertPoint(bodyBB);
            auto* curIdx = builder_->CreateLoad(idxType, idxAlloca, "idx");
            if (suffix == "raw") {
                // lucis_vec_ptr_raw returns void* pointing to element; load from it
                auto& dl     = module_->getDataLayout();
                auto  elemSz = dl.getTypeAllocSize(varType);
                auto* elemSzVal = llvm::ConstantInt::get(usizeTy, elemSz);
                auto ptrCallee = declareBuiltin("lucis_vec_ptr_raw", ptrTy,
                                               {ptrTy, usizeTy, usizeTy});
                auto* elemPtr = builder_->CreateCall(ptrCallee,
                                                     {vecAlloca, curIdx, elemSzVal}, "elem_ptr");
                auto* elem = builder_->CreateLoad(varType, elemPtr, "elem");
                builder_->CreateStore(elem, elemAlloca);
            } else {
                auto atCallee = declareBuiltin("lucis_vec_at_" + suffix, varType, {ptrTy, usizeTy});
                auto* elem = builder_->CreateCall(atCallee, {vecAlloca, curIdx}, "elem");
                builder_->CreateStore(elem, elemAlloca);
            }

            {
                auto savedLocals = locals_;
                loopBodyLocalsStack_.push_back(savedLocals);
                for (auto* stmt : ctx->block()->statement())
                    visit(stmt);
                emitBlockExitCleanups(savedLocals);
                loopBodyLocalsStack_.pop_back();
                locals_ = savedLocals;
            }
            if (!builder_->GetInsertBlock()->getTerminator())
                builder_->CreateBr(updateBB);

            // Update: idx++
            builder_->SetInsertPoint(updateBB);
            auto* curIdxUpd = builder_->CreateLoad(idxType, idxAlloca, "idx");
            auto* nextIdx   = builder_->CreateAdd(curIdxUpd,
                llvm::ConstantInt::get(idxType, 1), "inc");
            builder_->CreateStore(nextIdx, idxAlloca);
            builder_->CreateBr(condBB);

            loopStack_.pop_back();
            locals_.erase(varName);

            // Free temp vec from expression result
            builder_->SetInsertPoint(endBB);
            if (isTempVec) {
                auto* voidTy = llvm::Type::getVoidTy(*context_);
                auto freeCallee = declareBuiltin("lucis_vec_free_" + suffix, voidTy, {ptrTy});
                builder_->CreateCall(freeCallee, {vecAlloca});
            }
        } else if ((iterableTI && iterableTI->kind == TypeKind::Integer && iterableDims == 0) ||
                   (!iterableTI && resolveExprTypeInfo(iterExpr) &&
                    resolveExprTypeInfo(iterExpr)->kind == TypeKind::Integer)) {
            // Integer iteration: for TYPE i in N  =>  iterate 0..N-1
            auto* nVal = castValue(visit(iterExpr));
            auto* i64Ty = llvm::Type::getInt64Ty(*context_);

            auto* alloca = builder_->CreateAlloca(varType, nullptr, varName);
            builder_->CreateStore(llvm::ConstantInt::get(varType, 0), alloca);
            locals_[varName] = {alloca, typeInfo, 0};

            auto* condBB   = llvm::BasicBlock::Create(*context_, "for.cond", currentFunction_);
            auto* bodyBB   = llvm::BasicBlock::Create(*context_, "for.body", currentFunction_);
            auto* updateBB = llvm::BasicBlock::Create(*context_, "for.update", currentFunction_);
            auto* endBB    = llvm::BasicBlock::Create(*context_, "for.end", currentFunction_);

            loopStack_.push_back({endBB, updateBB});
            builder_->CreateBr(condBB);

            // Condition: i < N
            builder_->SetInsertPoint(condBB);
            auto* cur = builder_->CreateLoad(varType, alloca, varName);
            auto* castEnd = (nVal->getType() == varType) ? nVal :
                builder_->CreateIntCast(nVal, varType, typeInfo->isSigned, "cast");
            llvm::Value* cond;
            if (typeInfo->isSigned)
                cond = builder_->CreateICmpSLT(cur, castEnd, "cmp");
            else
                cond = builder_->CreateICmpULT(cur, castEnd, "cmp");
            builder_->CreateCondBr(cond, bodyBB, endBB);

            // Body
            builder_->SetInsertPoint(bodyBB);
            {
                auto savedLocals = locals_;
                loopBodyLocalsStack_.push_back(savedLocals);
                for (auto* stmt : ctx->block()->statement())
                    visit(stmt);
                emitBlockExitCleanups(savedLocals);
                loopBodyLocalsStack_.pop_back();
                locals_ = savedLocals;
            }
            if (!builder_->GetInsertBlock()->getTerminator())
                builder_->CreateBr(updateBB);

            // Update: i++
            builder_->SetInsertPoint(updateBB);
            auto* curVal = builder_->CreateLoad(varType, alloca, "cur");
            auto* next = builder_->CreateAdd(curVal,
                llvm::ConstantInt::get(varType, 1), "inc");
            builder_->CreateStore(next, alloca);
            builder_->CreateBr(condBB);

            loopStack_.pop_back();
            locals_.erase(varName);

            builder_->SetInsertPoint(endBB);
        } else {
        // Array iteration: for TYPE x in arr
        auto* arrVal = castValue(visit(iterExpr));

        // Get array length from the source variable
        unsigned arrLen = 0;
        if (auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(arrVal->getType()))
            arrLen = arrTy->getNumElements();

        // Find the source alloca for indexed access
        llvm::AllocaInst* arrAlloca = nullptr;
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(iterExpr)) {
            auto it = locals_.find(ident->IDENTIFIER()->getText());
            if (it != locals_.end())
                arrAlloca = it->second.alloca;
        }

        auto* idxType = llvm::Type::getInt64Ty(*context_);
        auto* idxAlloca = builder_->CreateAlloca(idxType, nullptr, "idx");
        builder_->CreateStore(llvm::ConstantInt::get(idxType, 0), idxAlloca);

            auto* elemAlloca = builder_->CreateAlloca(varType, nullptr, varName);
            VarInfo elemVI{ elemAlloca, typeInfo, 0 };
            // Elements are shallow copies owned by the vec — never auto-free
            // their string fields.
            if (typeInfo && typeInfo->kind == TypeKind::Struct)
                elemVI.isBorrowed = true;
            locals_[varName] = std::move(elemVI);

        auto* condBB   = llvm::BasicBlock::Create(*context_, "for.cond", currentFunction_);
        auto* bodyBB   = llvm::BasicBlock::Create(*context_, "for.body", currentFunction_);
        auto* updateBB = llvm::BasicBlock::Create(*context_, "for.update", currentFunction_);
        auto* endBB    = llvm::BasicBlock::Create(*context_, "for.end", currentFunction_);

        loopStack_.push_back({endBB, updateBB});

        builder_->CreateBr(condBB);

        // Condition: idx < arrLen
        builder_->SetInsertPoint(condBB);
        auto* idx = builder_->CreateLoad(idxType, idxAlloca, "idx");
        auto* cond = builder_->CreateICmpULT(idx,
            llvm::ConstantInt::get(idxType, arrLen), "cmp");
        builder_->CreateCondBr(cond, bodyBB, endBB);

        // Body: load element from array
        builder_->SetInsertPoint(bodyBB);
        auto* curIdx = builder_->CreateLoad(idxType, idxAlloca, "idx");
        llvm::Value* elemPtr;
        if (arrAlloca) {
            elemPtr = builder_->CreateInBoundsGEP(
                arrAlloca->getAllocatedType(), arrAlloca,
                {llvm::ConstantInt::get(idxType, 0), curIdx}, "elem_ptr");
        } else {
            elemPtr = builder_->CreateInBoundsGEP(
                varType, arrVal, {curIdx}, "elem_ptr");
        }
        auto* elem = builder_->CreateLoad(varType, elemPtr, "elem");
        builder_->CreateStore(elem, elemAlloca);

        {
            auto savedLocals = locals_;
            loopBodyLocalsStack_.push_back(savedLocals);
            for (auto* stmt : ctx->block()->statement())
                visit(stmt);
            emitBlockExitCleanups(savedLocals);
            loopBodyLocalsStack_.pop_back();
            locals_ = savedLocals;
        }
        if (!builder_->GetInsertBlock()->getTerminator())
            builder_->CreateBr(updateBB);

        // Update: idx++
        builder_->SetInsertPoint(updateBB);
        auto* curIdxUpdate = builder_->CreateLoad(idxType, idxAlloca, "idx");
        auto* nextIdx = builder_->CreateAdd(curIdxUpdate,
            llvm::ConstantInt::get(idxType, 1), "inc");
        builder_->CreateStore(nextIdx, idxAlloca);
        builder_->CreateBr(condBB);

        loopStack_.pop_back();
        locals_.erase(varName);

        builder_->SetInsertPoint(endBB);
        }
        } // close variadic else
    }

    return {};
}



std::any IRGen::visitBreakStmt(LucisParser::BreakStmtContext* /*ctx*/) {
    if (!loopStack_.empty()) {
        if (!loopBodyLocalsStack_.empty())
            emitBlockExitCleanups(loopBodyLocalsStack_.back());
        builder_->CreateBr(loopStack_.back().breakTarget);
    }
    return {};
}



std::any IRGen::visitContinueStmt(LucisParser::ContinueStmtContext* /*ctx*/) {
    if (!loopStack_.empty()) {
        if (!loopBodyLocalsStack_.empty())
            emitBlockExitCleanups(loopBodyLocalsStack_.back());
        builder_->CreateBr(loopStack_.back().continueTarget);
    }
    return {};
}



std::any IRGen::visitLoopStmt(LucisParser::LoopStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* bodyBB = llvm::BasicBlock::Create(*context_, "loop.body", currentFunction_);
    auto* endBB  = llvm::BasicBlock::Create(*context_, "loop.end", currentFunction_);

    loopStack_.push_back({endBB, bodyBB});

    builder_->CreateBr(bodyBB);

    builder_->SetInsertPoint(bodyBB);
    {
        auto savedLocals = locals_;
        loopBodyLocalsStack_.push_back(savedLocals);
        for (auto* stmt : ctx->block()->statement())
            visit(stmt);
        emitBlockExitCleanups(savedLocals);
        loopBodyLocalsStack_.pop_back();
        locals_ = savedLocals;
    }
    if (!builder_->GetInsertBlock()->getTerminator())
        builder_->CreateBr(bodyBB);

    loopStack_.pop_back();

    builder_->SetInsertPoint(endBB);
    return {};
}



std::any IRGen::visitSwitchStmt(LucisParser::SwitchStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    // Evaluate the switch expression
    auto* switchVal = castValue(visit(ctx->expression()));

    auto* mergeBB = llvm::BasicBlock::Create(*context_, "switch.end", currentFunction_);

    // Default block: either the user-defined default or merge
    llvm::BasicBlock* defaultBB = mergeBB;
    if (ctx->defaultClause()) {
        defaultBB = llvm::BasicBlock::Create(*context_, "switch.default", currentFunction_);
    }

    auto caseClauses = ctx->caseClause();
    auto* switchInst = builder_->CreateSwitch(switchVal, defaultBB,
                                              static_cast<unsigned>(caseClauses.size()));

    // Emit each case
    for (auto* cc : caseClauses) {
        auto* caseBB = llvm::BasicBlock::Create(*context_, "switch.case", currentFunction_);

        // Each case can have multiple values: case 1, 2, 3 { ... }
        auto caseExprs = cc->expression();
        for (auto* caseExpr : caseExprs) {
            auto* caseVal = castValue(visit(caseExpr));
            // Promote to match switch value width
            if (caseVal->getType() != switchVal->getType())
                caseVal = builder_->CreateIntCast(caseVal, switchVal->getType(), true);
            auto* constVal = llvm::dyn_cast<llvm::ConstantInt>(caseVal);
            switchInst->addCase(constVal, caseBB);
        }

        // Emit case body
        builder_->SetInsertPoint(caseBB);
        {
            auto savedLocals = locals_;
            for (auto* stmt : cc->block()->statement())
                visit(stmt);
            emitBlockExitCleanups(savedLocals);
            locals_ = savedLocals;
        }
        if (!builder_->GetInsertBlock()->getTerminator())
            builder_->CreateBr(mergeBB);
    }

    // Emit default body
    if (ctx->defaultClause()) {
        builder_->SetInsertPoint(defaultBB);
        {
            auto savedLocals = locals_;
            for (auto* stmt : ctx->defaultClause()->block()->statement())
                visit(stmt);
            emitBlockExitCleanups(savedLocals);
            locals_ = savedLocals;
        }
        if (!builder_->GetInsertBlock()->getTerminator())
            builder_->CreateBr(mergeBB);
    }

    builder_->SetInsertPoint(mergeBB);
    return {};
}



std::any IRGen::visitWhileStmt(LucisParser::WhileStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* condBB = llvm::BasicBlock::Create(*context_, "while.cond", currentFunction_);
    auto* bodyBB = llvm::BasicBlock::Create(*context_, "while.body", currentFunction_);
    auto* endBB  = llvm::BasicBlock::Create(*context_, "while.end", currentFunction_);

    loopStack_.push_back({endBB, condBB});

    builder_->CreateBr(condBB);

    // Condition
    builder_->SetInsertPoint(condBB);
    auto* cond = castValue(visit(ctx->expression()));
    cond = toBool(cond, resolveExprTypeInfo(ctx->expression()), "tobool");
    builder_->CreateCondBr(cond, bodyBB, endBB);

    // Body
    builder_->SetInsertPoint(bodyBB);
    {
        auto savedLocals = locals_;
        loopBodyLocalsStack_.push_back(savedLocals);
        for (auto* stmt : ctx->block()->statement())
            visit(stmt);
        emitBlockExitCleanups(savedLocals);
        loopBodyLocalsStack_.pop_back();
        locals_ = savedLocals;
    }
    if (!builder_->GetInsertBlock()->getTerminator())
        builder_->CreateBr(condBB);

    loopStack_.pop_back();

    builder_->SetInsertPoint(endBB);
    return {};
}



std::any IRGen::visitDoWhileStmt(LucisParser::DoWhileStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* bodyBB = llvm::BasicBlock::Create(*context_, "do.body", currentFunction_);
    auto* condBB = llvm::BasicBlock::Create(*context_, "do.cond", currentFunction_);
    auto* endBB  = llvm::BasicBlock::Create(*context_, "do.end", currentFunction_);

    loopStack_.push_back({endBB, condBB});

    builder_->CreateBr(bodyBB);

    // Body (executes at least once)
    builder_->SetInsertPoint(bodyBB);
    {
        auto savedLocals = locals_;
        loopBodyLocalsStack_.push_back(savedLocals);
        for (auto* stmt : ctx->block()->statement())
            visit(stmt);
        emitBlockExitCleanups(savedLocals);
        loopBodyLocalsStack_.pop_back();
        locals_ = savedLocals;
    }
    if (!builder_->GetInsertBlock()->getTerminator())
        builder_->CreateBr(condBB);

    // Condition
    builder_->SetInsertPoint(condBB);
    auto* cond = castValue(visit(ctx->expression()));
    cond = toBool(cond, resolveExprTypeInfo(ctx->expression()), "tobool");
    builder_->CreateCondBr(cond, bodyBB, endBB);

    loopStack_.pop_back();

    builder_->SetInsertPoint(endBB);
    return {};
}



std::any IRGen::visitDerefAssignStmt(LucisParser::DerefAssignStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    llvm::Value* ptrVal = nullptr;
    const TypeInfo* ptrTI = nullptr;
    LucisParser::ExpressionContext* rhsExpr = nullptr;

    if (ctx->IDENTIFIER()) {
        // *ptr = value;
        auto varName = ctx->IDENTIFIER()->getText();
        auto it = locals_.find(varName);
        if (it == locals_.end()) {
            std::cerr << "lucis: undefined variable '" << varName << "'\n";
            return {};
        }

        auto* alloca = it->second.alloca;
        ptrTI = it->second.typeInfo;

        if (ptrTI->kind != TypeKind::Pointer) {
            std::cerr << "lucis: '" << varName << "' is not a pointer\n";
            return {};
        }

        ptrVal = builder_->CreateLoad(
            llvm::PointerType::getUnqual(*context_), alloca, varName + "_load");
        rhsExpr = ctx->expression(0);
    } else {
        // *(expr) = value;
        ptrVal = castValue(visit(ctx->expression(0)));

        // Resolve pointer TypeInfo from the expression
        auto* exprTI = resolveExprTypeInfo(ctx->expression(0));
        if (exprTI && exprTI->kind == TypeKind::Pointer) {
            ptrTI = exprTI;
        }

        rhsExpr = ctx->expression(1);
    }

    // Evaluate RHS
    auto* val = castValue(visit(rhsExpr));

    // Cast if needed
    if (ptrTI && ptrTI->pointeeType) {
        auto* pointeeTy = ptrTI->pointeeType->toLLVMType(*context_, module_->getDataLayout());
        if (val->getType() != pointeeTy) {
            if (val->getType()->isIntegerTy() && pointeeTy->isIntegerTy())
                val = builder_->CreateIntCast(val, pointeeTy, ptrTI->pointeeType->isSigned);
            else if (val->getType()->isFloatingPointTy() && pointeeTy->isFloatingPointTy()) {
                if (val->getType()->getPrimitiveSizeInBits() > pointeeTy->getPrimitiveSizeInBits())
                    val = builder_->CreateFPTrunc(val, pointeeTy);
                else
                    val = builder_->CreateFPExt(val, pointeeTy);
            }
        }
    }

    {
        bool isVol = ptrTI && ptrTI->pointeeType && ptrTI->pointeeType->isVolatile;
        auto* store = builder_->CreateStore(val, ptrVal);
        if (isVol) store->setVolatile(true);
    }
    return {};
}



std::any IRGen::visitDerefCompoundAssignStmt(LucisParser::DerefCompoundAssignStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    llvm::Value* ptrVal = nullptr;
    const TypeInfo* ptrTI = nullptr;
    LucisParser::ExpressionContext* rhsExpr = nullptr;

    if (ctx->IDENTIFIER()) {
        // *ptr op= value;
        auto varName = ctx->IDENTIFIER()->getText();
        auto it = locals_.find(varName);
        if (it == locals_.end()) {
            std::cerr << "lucis: undefined variable '" << varName << "'\n";
            return {};
        }

        auto* alloca = it->second.alloca;
        ptrTI = it->second.typeInfo;
        if (!ptrTI || ptrTI->kind != TypeKind::Pointer || !ptrTI->pointeeType) {
            std::cerr << "lucis: '" << varName << "' is not a pointer\n";
            return {};
        }

        ptrVal = builder_->CreateLoad(
            llvm::PointerType::getUnqual(*context_), alloca, varName + "_load");
        rhsExpr = ctx->expression(0);
    } else {
        // *(expr) op= value;
        ptrVal = castValue(visit(ctx->expression(0)));
        auto* exprTI = resolveExprTypeInfo(ctx->expression(0));
        if (exprTI && exprTI->kind == TypeKind::Pointer)
            ptrTI = exprTI;

        rhsExpr = ctx->expression(1);
    }

    if (!ptrTI || !ptrTI->pointeeType) {
        std::cerr << "lucis: cannot dereference non-pointer expression\n";
        return {};
    }

    auto* pointeeTy = ptrTI->pointeeType->toLLVMType(*context_, module_->getDataLayout());
    auto* cur = builder_->CreateLoad(pointeeTy, ptrVal, "deref_compound_cur");
    if (ptrTI->pointeeType->isVolatile)
        llvm::cast<llvm::LoadInst>(cur)->setVolatile(true);
    auto* rhs = castValue(visit(rhsExpr));

    if (rhs->getType() != pointeeTy) {
        if (rhs->getType()->isIntegerTy() && pointeeTy->isIntegerTy())
            rhs = builder_->CreateIntCast(rhs, pointeeTy, ptrTI->pointeeType->isSigned);
        else if (rhs->getType()->isFloatingPointTy() && pointeeTy->isFloatingPointTy()) {
            if (rhs->getType()->getPrimitiveSizeInBits() > pointeeTy->getPrimitiveSizeInBits())
                rhs = builder_->CreateFPTrunc(rhs, pointeeTy);
            else
                rhs = builder_->CreateFPExt(rhs, pointeeTy);
        }
    }

    // ── Null-coalescing: *ptr ??= default (ptr must be pointer-to-pointer) ─
    if (ctx->op->getType() == LucisLexer::NULLCOAL_ASSIGN) {
        auto* isNull = builder_->CreateICmpEQ(
            cur, llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_)),
            "ncoal_isnull");
        auto* thenBB = llvm::BasicBlock::Create(*context_, "ncoal.store", currentFunction_);
        auto* mergeBB = llvm::BasicBlock::Create(*context_, "ncoal.end", currentFunction_);
        builder_->CreateCondBr(isNull, thenBB, mergeBB);
        builder_->SetInsertPoint(thenBB);
        builder_->CreateStore(rhs, ptrVal);
        builder_->CreateBr(mergeBB);
        builder_->SetInsertPoint(mergeBB);
        return {};
    }

    bool isFloat = pointeeTy->isFloatingPointTy();
    llvm::Value* result = nullptr;

    if (!isFloat && (ctx->op->getType() == LucisLexer::SLASH_ASSIGN ||
                     ctx->op->getType() == LucisLexer::PERCENT_ASSIGN)) {
        emitDivByZeroGuard(rhs, ctx->op);
    }

    switch (ctx->op->getType()) {
    case LucisLexer::PLUS_ASSIGN:
        result = isFloat ? builder_->CreateFAdd(cur, rhs) : builder_->CreateAdd(cur, rhs);
        break;
    case LucisLexer::MINUS_ASSIGN:
        result = isFloat ? builder_->CreateFSub(cur, rhs) : builder_->CreateSub(cur, rhs);
        break;
    case LucisLexer::STAR_ASSIGN:
        result = isFloat ? builder_->CreateFMul(cur, rhs) : builder_->CreateMul(cur, rhs);
        break;
    case LucisLexer::SLASH_ASSIGN:
        result = isFloat ? builder_->CreateFDiv(cur, rhs) : builder_->CreateSDiv(cur, rhs);
        break;
    case LucisLexer::PERCENT_ASSIGN:
        result = isFloat ? builder_->CreateFRem(cur, rhs) : builder_->CreateSRem(cur, rhs);
        break;
    case LucisLexer::AMP_ASSIGN:
        result = builder_->CreateAnd(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    case LucisLexer::PIPE_ASSIGN:
        result = builder_->CreateOr(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    case LucisLexer::CARET_ASSIGN:
        result = builder_->CreateXor(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    case LucisLexer::LSHIFT_ASSIGN:
        result = builder_->CreateShl(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    case LucisLexer::RSHIFT_ASSIGN:
        result = ptrTI->pointeeType->isSigned ? builder_->CreateAShr(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs))
                                              : builder_->CreateLShr(ptrToIntIfNeeded(cur), ptrToIntIfNeeded(rhs));
        break;
    default:
        result = cur;
        break;
    }

    {
        auto* store = builder_->CreateStore(result, ptrVal);
        if (ptrTI->pointeeType->isVolatile) store->setVolatile(true);
    }
    return {};
}

// ── Macro constant evaluator helpers (for function-like macros) ──
// C operator precedence (lowest to highest within this subset):
//   shift (<< >>) → additive (+ -) → multiplicative (* / %) → primary


std::any IRGen::visitDeferStmt(LucisParser::DeferStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    DeferredStmt ds;
    if (ctx->callStmt())
        ds.callCtx = ctx->callStmt();
    else if (ctx->exprStmt())
        ds.exprCtx = ctx->exprStmt();
    deferStack_.push_back(ds);
    return {};
}

// ── Argument count check for builtins ──────────────────────────────────


bool IRGen::requireArgs(const std::string& funcName,
                        const std::vector<llvm::Value*>& args,
                        size_t expected) {
    if (args.size() >= expected) return true;
    std::cerr << "lucis: internal error: '" << funcName << "' requires "
              << expected << " argument(s), got " << args.size() << "\n";
    return false;
}

// ── Division-by-zero runtime guard ─────────────────────────────────────
// Emits a conditional branch: if divisor == 0, throw "division by zero".
// After this call, the insert point is at the "safe" basic block.


void IRGen::emitDivByZeroGuard(llvm::Value* divisor, antlr4::Token* opToken) {
    auto* zero    = llvm::ConstantInt::get(divisor->getType(), 0);
    auto* isZero  = builder_->CreateICmpEQ(divisor, zero, "div_zero_chk");

    auto* fn      = currentFunction_;
    auto* throwBB = llvm::BasicBlock::Create(*context_, "div.throw", fn);
    auto* okBB    = llvm::BasicBlock::Create(*context_, "div.ok",    fn);

    builder_->CreateCondBr(isZero, throwBB, okBB);

    builder_->SetInsertPoint(throwBB);
    {
        auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
        auto* voidTy  = llvm::Type::getVoidTy(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* i32Ty   = llvm::Type::getInt32Ty(*context_);
        auto* errorTy = llvm::StructType::getTypeByName(*context_, "Error");
        auto* strTy   = llvm::StructType::get(*context_, { ptrTy, usizeTy });

        auto* msgGlobal = builder_->CreateGlobalString(
            "division by zero", ".div_err_msg", 0, module_);
        auto* msgLen = llvm::ConstantInt::get(usizeTy, 17);
        llvm::Value* msgStr = llvm::UndefValue::get(strTy);
        msgStr = builder_->CreateInsertValue(msgStr, msgGlobal, {0});
        msgStr = builder_->CreateInsertValue(msgStr, msgLen,    {1});

        std::string fileName = module_->getSourceFileName();
        auto* fileGlobal = builder_->CreateGlobalString(
            fileName, ".div_err_file", 0, module_);
        auto* fileLen = llvm::ConstantInt::get(usizeTy, fileName.size());
        llvm::Value* fileStr = llvm::UndefValue::get(strTy);
        fileStr = builder_->CreateInsertValue(fileStr, fileGlobal, {0});
        fileStr = builder_->CreateInsertValue(fileStr, fileLen,    {1});

        auto* line = llvm::ConstantInt::get(i32Ty, opToken->getLine());
        auto* col  = llvm::ConstantInt::get(i32Ty,
                         opToken->getCharPositionInLine());

        llvm::Value* errVal = llvm::ConstantAggregateZero::get(errorTy);
        errVal = builder_->CreateInsertValue(errVal, msgStr,  {0});
        errVal = builder_->CreateInsertValue(errVal, fileStr, {1});
        errVal = builder_->CreateInsertValue(errVal, line,    {2});
        errVal = builder_->CreateInsertValue(errVal, col,     {3});

        auto* errAlloca = builder_->CreateAlloca(errorTy, nullptr, "div_err");
        builder_->CreateStore(errVal, errAlloca);

        auto throwFn = declareBuiltin("lucis_eh_throw", voidTy, {ptrTy});
        builder_->CreateCall(throwFn, {errAlloca});
        builder_->CreateUnreachable();
    }

    builder_->SetInsertPoint(okBB);
}



void IRGen::emitBoundsCheck(llvm::Value* index, llvm::Value* length,
                             const std::string& name) {
    auto* ok = builder_->CreateICmpULT(index, length, name + ".bnd.chk");
    auto* fn = currentFunction_;
    auto* panicBB = llvm::BasicBlock::Create(*context_, name + ".bnd.panic", fn);
    auto* okBB    = llvm::BasicBlock::Create(*context_, name + ".bnd.ok",    fn);
    builder_->CreateCondBr(ok, okBB, panicBB);

    builder_->SetInsertPoint(panicBB);
    auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
    auto* voidTy  = llvm::Type::getVoidTy(*context_);
    auto* msg     = builder_->CreateGlobalString(
        "index out of bounds", name + ".bnd.msg", 0, module_);
    auto* msgLen  = llvm::ConstantInt::get(usizeTy, 20);
    auto panicFn  = declareBuiltin("lucis_panic", voidTy, {ptrTy, usizeTy});
    builder_->CreateCall(panicFn, {msg, msgLen});
    builder_->CreateUnreachable();

    builder_->SetInsertPoint(okBB);
}



void IRGen::emitDeferredCleanups() {
    for (auto it = deferStack_.rbegin(); it != deferStack_.rend(); ++it) {
        emitOneDeferred(*it);
    }
}



void IRGen::emitOneDeferred(const DeferredStmt& ds) {
    if (ds.callCtx)
        visit(ds.callCtx);
    else if (ds.exprCtx)
        visit(ds.exprCtx);
    else if (ds.scopeCbCtx)
        emitScopeCallback(ds.scopeCbCtx);
}

// ── Structural Blocks ────────────────────────────────────────────────────────

// { statements }  —  lexical scope block (no callbacks, just inline statements)


std::any IRGen::visitNakedBlockStmt(LucisParser::NakedBlockStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    auto savedLocals = locals_;
    size_t savedDeferBase = deferStack_.size();
    for (auto* stmt : ctx->statement())
        visit(stmt);
    auto* bb = builder_->GetInsertBlock();
    if (bb && !bb->getTerminator()) {
        for (size_t i = deferStack_.size(); i-- > savedDeferBase;)
            emitOneDeferred(deferStack_[i]);
    }
    deferStack_.resize(savedDeferBase);
    emitBlockExitCleanups(savedLocals);
    locals_ = savedLocals;
    return {};
}

// #inline { statements }  —  inject statements directly into parent scope


std::any IRGen::visitInlineBlockStmt(LucisParser::InlineBlockStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    for (auto* stmt : ctx->statement())
        visit(stmt);
    return {};
}

// #scope (A(), B()) { statements }  —  RAII block with guaranteed exit callbacks
// Callbacks execute in LIFO order on every exit path (normal + early return).


std::any IRGen::visitScopeBlockStmt(LucisParser::ScopeBlockStmtContext* ctx) {
        SET_DBG_LOC(ctx);
    // Record stack depth before pushing this scope's callbacks.
    size_t deferBase = deferStack_.size();

    // Push callbacks in declaration order; LIFO emission gives reverse execution.
    if (auto* cbs = ctx->scopeCallbackList()) {
        for (auto* cb : cbs->scopeCallback()) {
            DeferredStmt ds;
            ds.scopeCbCtx = cb;
            deferStack_.push_back(ds);
        }
    }

    // Visit block body.
    auto savedLocals = locals_;
    for (auto* stmt : ctx->statement())
        visit(stmt);

    // Normal-exit path: emit callbacks in LIFO order and pop them.
    // Early-return path: emitAllCleanups() (called by visitReturnStmt) already
    // emitted them, so we just pop without re-emitting.
    // Callbacks are emitted while body locals are still visible.
    auto* bb = builder_->GetInsertBlock();
    if (bb && !bb->getTerminator()) {
        for (size_t i = deferStack_.size(); i-- > deferBase;)
            emitScopeCallback(deferStack_[i].scopeCbCtx);
        emitBlockExitCleanups(savedLocals);
    }
    deferStack_.resize(deferBase);
    locals_ = savedLocals;

    return {};
}

// ── c_macro { ... } block ──────────────────────────────────────────────


std::any IRGen::visitCMacroBlock(LucisParser::CMacroBlockContext* ctx) {
        SET_DBG_LOC(ctx);
    auto tok = ctx->C_MACRO_BLOCK();
    if (!tok) return {};
    std::string fullText = tok->getText();
    if (fullText.size() < 11) return {};
    std::string rawC = fullText.substr(9, fullText.size() - 10);

    std::string tempDir = "/tmp";
    CBindings localBindings;
    evalCMacroRaw(rawC, currentFile_, tok->getSymbol()->getLine(),
                  tempDir, localBindings, false);

    for (auto& [name, cm] : localBindings.macros()) {
        if (cMacroBindings_.findMacro(name)) continue;
        cMacroBindings_.addMacro(cm);
        cEnumConstants_[name] = cm.value;
    }
    for (auto& [name, flm] : localBindings.functionLikeMacros()) {
        if (cMacroBindings_.findFunctionLikeMacro(name)) continue;
        cMacroBindings_.addFunctionLikeMacro(flm);
    }
    return {};
}



std::any IRGen::visitAsmBBlock(LucisParser::AsmBBlockContext* ctx) {
    auto tok = ctx->ASM_B_BLOCK();
    if (!tok) return {};
    std::string fullText = tok->getText();
    // fullText = 'asm_b "filename" { raw assembly }'
    auto firstQuote = fullText.find('"');
    if (firstQuote == std::string::npos) return {};
    auto secondQuote = fullText.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos) return {};
    std::string filename = fullText.substr(firstQuote + 1, secondQuote - firstQuote - 1);
    if (filename.empty()) return {};
    auto braceOpen = fullText.find('{', secondQuote);
    if (braceOpen == std::string::npos) return {};
    std::string rawAsm = fullText.substr(braceOpen + 1, fullText.size() - braceOpen - 2);
    std::string asmDir;
    if (!projectRoot_.empty())
        asmDir = (std::filesystem::path(projectRoot_) / ".lucis" / "asm_b").string();
    else
        asmDir = "/tmp/lucis_asm_b";
    std::filesystem::create_directories(asmDir);
    // Sanitize filename: strip directory components to prevent path traversal
    std::string safeFilename = std::filesystem::path(filename).filename().string();
    if (safeFilename.empty()) return {};
    std::string asmPath = (std::filesystem::path(asmDir) / safeFilename).string();
    {
        std::ofstream f(asmPath);
        if (f.is_open()) { f << rawAsm; f.close(); }
        else { std::cerr << "lucis: warning: could not write inline assembly to " << asmPath << "\n"; return {}; }
    }
    inlineAssemblyFiles_.push_back({asmPath});
    return {};
}

// Emit a single #scope callback: IDENTIFIER LPAREN argList? RPAREN


void IRGen::emitScopeCallback(LucisParser::ScopeCallbackContext* ctx) {
    // ── Dot-access method call: id1.drop() ───────────────────────────────────
    if (ctx->DOT()) {
        auto varName    = ctx->IDENTIFIER(0)->getText();
        auto methodName = ctx->IDENTIFIER(1)->getText();

        auto locIt = locals_.find(varName);
        if (locIt == locals_.end()) {
            std::cerr << "lucis: #scope callback: unknown variable '" << varName << "'\n";
            return;
        }

        auto* recvTI = locIt->second.typeInfo;

        // ── Struct extend method ──────────────────────────────────────────────
        if (recvTI && recvTI->kind == TypeKind::Struct) {
            auto* me = findMethodEntryInChain(recvTI, methodName);
            if (!me) {
                std::cerr << "lucis: #scope callback: unknown method '" << methodName
                          << "' on '" << recvTI->name << "'\n";
                return;
            }

            auto* fn = me->fn;
            std::vector<llvm::Value*> callArgs = { locIt->second.alloca };

            if (auto* argList = ctx->argList()) {
                auto* fnType = fn->getFunctionType();
                size_t paramIdx = 1; // skip &self
                for (auto* argExpr : argList->expression()) {
                    auto* argVal = castValue(visit(argExpr));
                    if (paramIdx < fnType->getNumParams()) {
                        auto* paramTy = fnType->getParamType(paramIdx);
                        if (argVal->getType() != paramTy) {
                            if (argVal->getType()->isIntegerTy() && paramTy->isIntegerTy())
                                argVal = builder_->CreateIntCast(argVal, paramTy, true);
                            else if (argVal->getType()->isFloatingPointTy() && paramTy->isFloatingPointTy())
                                argVal = builder_->CreateFPCast(argVal, paramTy);
                        }
                    }
                    callArgs.push_back(argVal);
                    ++paramIdx;
                }
            }

            builder_->CreateCall(fn, callArgs);
            return;
        }

        // ── Extended type method (Vec/Map/Set) ────────────────────────────────
        if (recvTI && recvTI->kind == TypeKind::Extended) {
            auto* extDesc = extTypeRegistry_.lookup(recvTI->extendedKind);
            if (!extDesc) {
                std::cerr << "lucis: #scope callback: unknown extended type '"
                          << recvTI->extendedKind << "'\n";
                return;
            }
            const MethodDescriptor* desc = nullptr;
            for (auto& md : extDesc->methods) {
                if (md.name == methodName) { desc = &md; break; }
            }
            if (!desc) {
                std::cerr << "lucis: #scope callback: unknown method '" << methodName
                          << "' on extended type '" << recvTI->name << "'\n";
                return;
            }

            auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
            auto* voidTy  = llvm::Type::getVoidTy(*context_);

            std::string suffix;
            if (!recvTI->builtinSuffix.empty())
                suffix = recvTI->builtinSuffix;
            else if (recvTI->elementType)
                suffix = getVecSuffix(recvTI->elementType);
            else
                suffix = "raw";

            auto cFuncName = extDesc->cPrefix + "_" + methodName + "_" + suffix;

            std::vector<llvm::Value*> callArgs = { locIt->second.alloca };
            if (auto* argList = ctx->argList()) {
                for (auto* argExpr : argList->expression())
                    callArgs.push_back(castValue(visit(argExpr)));
            }

            auto callee = declareBuiltin(cFuncName, voidTy, {ptrTy});
            builder_->CreateCall(callee, callArgs);
            return;
        }

        std::cerr << "lucis: #scope callback: '" << varName
                  << "' has no method '" << methodName << "'\n";
        return;
    }

    // ── Plain function call: funcName(args) ───────────────────────────────────
    auto funcName = ctx->IDENTIFIER(0)->getText();

    auto resolvedName = resolveCallTarget(funcName);
    auto* fn = module_->getFunction(resolvedName);
    if (!fn)
        fn = declareCFunction(funcName);
    if (!fn) {
        std::cerr << "lucis: #scope callback: unknown function '" << funcName << "'\n";
        return;
    }

    std::vector<llvm::Value*> args;
    if (auto* argList = ctx->argList()) {
        auto* fnTy = fn->getFunctionType();
        auto params = fnTy->params();
        size_t idx = 0;
        for (auto* argExpr : argList->expression()) {
            auto* v = castValue(visit(argExpr));
            // Coerce to declared parameter type when possible.
            if (idx < params.size() && v->getType() != params[idx]) {
                if (v->getType()->isIntegerTy() && params[idx]->isIntegerTy())
                    v = builder_->CreateIntCast(v, params[idx], true);
                else if (v->getType()->isFloatingPointTy() && params[idx]->isFloatingPointTy()) {
                    if (v->getType()->getPrimitiveSizeInBits() > params[idx]->getPrimitiveSizeInBits())
                        v = builder_->CreateFPTrunc(v, params[idx]);
                    else
                        v = builder_->CreateFPExt(v, params[idx]);
                }
            }
            args.push_back(v);
            ++idx;
        }
    }

    builder_->CreateCall(fn, args);
}



void IRGen::emitAutoCleanups(const std::string& skipVar) {
    for (auto& [name, info] : locals_) {
        if (name == skipVar) continue;
        emitCleanupForLocal(name, info);
    }
}



void IRGen::emitCleanupForLocal(const std::string& name, const VarInfo& info) {
    if (info.consumed) return;
    // Parameters are shallow copies borrowed from the caller — the caller
    // owns the data (strings, Vec/Map/Set buffers, struct string fields)
    // and frees it at its own scope exit. Freeing here double-frees.
    if (info.isParam) return;
    if (!info.typeInfo) return;
    if (info.arrayDims > 0) return;
    auto* voidTy = llvm::Type::getVoidTy(*context_);
    auto* ptrTy  = llvm::PointerType::getUnqual(*context_);
    auto* usizeTy = typeRegistry_.lookup("usize")->toLLVMType(*context_, module_->getDataLayout());

    if (info.typeInfo->kind == TypeKind::String) {
        if (info.isBorrowed) return;
        auto* value = builder_->CreateLoad(info.alloca->getAllocatedType(), info.alloca, name + "_str");
        auto* strPtr = builder_->CreateExtractValue(value, 0, name + "_ptr");
        auto* strLen = builder_->CreateExtractValue(value, 1, name + "_len");
        auto callee = declareBuiltin("lucis_freeStr", voidTy, {ptrTy, usizeTy});
        builder_->CreateCall(callee, {strPtr, strLen});
        return;
    }

    // ── User-defined drop() method ───────────────────────────────────
    if (info.typeInfo->dropTracked && !info.isBorrowed) {
        auto* me = findMethodEntryInChain(info.typeInfo, "drop");
        if (me && me->fn) {
            builder_->CreateCall(me->fn, {info.alloca});
        }
        // Fall through to struct/extended cleanup to free Vec/Map/Set fields
    }

    // ── Struct cleanup: free owned string fields and Extended type fields ─────
    if (info.typeInfo->kind == TypeKind::Struct) {
        // Structs copied from a Vec element (at()/for-in) are shallow copies
        // — the vec owns the field memory; freeing here double-frees.
        if (info.isBorrowed) return;
        auto* structLLTy = info.typeInfo->toLLVMType(*context_, module_->getDataLayout());
        for (size_t fieldIdx = 0; fieldIdx < info.typeInfo->fields.size(); fieldIdx++) {
            auto& field = info.typeInfo->fields[fieldIdx];
            auto* fieldPtr = builder_->CreateStructGEP(structLLTy, info.alloca, fieldIdx, field.name + "_gep");
            if (field.typeInfo && field.typeInfo->kind == TypeKind::String && field.arrayDims == 0) {
                auto* fieldLLTy = field.typeInfo->toLLVMType(*context_, module_->getDataLayout());
                auto* fieldVal = builder_->CreateLoad(fieldLLTy, fieldPtr, field.name + "_val");
                auto* strPtr = builder_->CreateExtractValue(fieldVal, 0, field.name + "_ptr");
                auto* strLen = builder_->CreateExtractValue(fieldVal, 1, field.name + "_len");
                auto callee = declareBuiltin("lucis_freeStr", voidTy, {ptrTy, usizeTy});
                builder_->CreateCall(callee, {strPtr, strLen});
            } else if (field.typeInfo && field.typeInfo->kind == TypeKind::Extended) {
                // Vec/Map/Set field cleanup
                std::string fieldFreeFuncName;
                if (field.typeInfo->extendedKind == "Vec") {
                    auto elemTI = field.typeInfo->elementType ? field.typeInfo->elementType : field.typeInfo;
                    auto suffix = getVecSuffix(elemTI);
                    fieldFreeFuncName = "lucis_vec_free_" + suffix;
                } else if (field.typeInfo->extendedKind == "Map") {
                    fieldFreeFuncName = "lucis_map_free_" + field.typeInfo->builtinSuffix;
                } else if (field.typeInfo->extendedKind == "Set") {
                    auto suffix = field.typeInfo->elementType
                        ? (field.typeInfo->elementType->builtinSuffix.empty() ? "raw" : field.typeInfo->elementType->builtinSuffix)
                        : field.typeInfo->builtinSuffix;
                    fieldFreeFuncName = "lucis_set_free_" + suffix;
                }
                if (!fieldFreeFuncName.empty()) {
                    auto freeCallee = declareBuiltin(fieldFreeFuncName, voidTy, {ptrTy});
                    builder_->CreateCall(freeCallee, {fieldPtr});
                }
            }
        }
        return;
    }

    if (info.typeInfo->kind != TypeKind::Extended) return;

    // ── Vec/Map/Set cleanup ───────────────────────────────────────────────
    std::string freeFuncName;
    if (info.typeInfo->extendedKind == "Vec") {
        auto elemTI = info.typeInfo->elementType
                          ? info.typeInfo->elementType
                          : info.typeInfo;
        auto suffix = getVecSuffix(elemTI);

        // For Vec<struct_with_strings>: generate per-element string cleanup
        if (suffix == "raw" && elemTI->kind == TypeKind::Struct) {
            bool hasStringField = false;
            for (auto& field : elemTI->fields) {
                if (field.typeInfo && field.typeInfo->kind == TypeKind::String && field.arrayDims == 0) {
                    hasStringField = true;
                    break;
                }
            }
            if (hasStringField) {
                auto& dl = module_->getDataLayout();
                auto* elemLLTy = elemTI->toLLVMType(*context_, dl);
                auto  elemSz = dl.getTypeAllocSize(elemLLTy);
                auto* elemSzVal = llvm::ConstantInt::get(usizeTy, elemSz);
                auto* i1Ty = llvm::Type::getInt1Ty(*context_);
                auto  structFields = elemTI->fields;

                // Load vec header fields
                auto* vecTy = getOrCreateVecStructType();
                auto* vecVal = builder_->CreateLoad(vecTy, info.alloca, name + "_vec");
                auto* vecPtr = builder_->CreateExtractValue(vecVal, 0, name + "_vec_ptr");
                auto* vecLen = builder_->CreateExtractValue(vecVal, 1, name + "_vec_len");

                auto* condBB  = llvm::BasicBlock::Create(*context_, "vec.cln.cond", currentFunction_);
                auto* bodyBB  = llvm::BasicBlock::Create(*context_, "vec.cln.body", currentFunction_);
                auto* endBB   = llvm::BasicBlock::Create(*context_, "vec.cln.end", currentFunction_);

                auto* zero = llvm::ConstantInt::get(usizeTy, 0);
                auto* one  = llvm::ConstantInt::get(usizeTy, 1);
                auto* idxPtr = builder_->CreateAlloca(usizeTy, nullptr, name + "_vec_cln_idx");
                builder_->CreateStore(zero, idxPtr);
                builder_->CreateBr(condBB);

                builder_->SetInsertPoint(condBB);
                auto* idx = builder_->CreateLoad(usizeTy, idxPtr, "cln_idx");
                auto* done = builder_->CreateICmpEQ(idx, vecLen, "cln_done");
                builder_->CreateCondBr(done, endBB, bodyBB);

                builder_->SetInsertPoint(bodyBB);
                auto* bytePtr = builder_->CreateGEP(llvm::Type::getInt8Ty(*context_), vecPtr,
                    builder_->CreateMul(idx, elemSzVal), "cln_elem_byte");
                auto* elemPtr = builder_->CreateBitCast(bytePtr, llvm::PointerType::getUnqual(*context_), "cln_elem_ptr");
                auto* elemVal = builder_->CreateLoad(elemLLTy, elemPtr, "cln_elem");

                for (size_t fieldIdx = 0; fieldIdx < structFields.size(); fieldIdx++) {
                    auto& f = structFields[fieldIdx];
                    if (f.typeInfo && f.typeInfo->kind == TypeKind::String && f.arrayDims == 0) {
                        auto* fv = builder_->CreateExtractValue(elemVal, fieldIdx, f.name + "_fval");
                        auto* fp = builder_->CreateExtractValue(fv, 0, f.name + "_fptr");
                        auto* fl = builder_->CreateExtractValue(fv, 1, f.name + "_flen");
                        auto callee = declareBuiltin("lucis_freeStr", voidTy, {ptrTy, usizeTy});
                        builder_->CreateCall(callee, {fp, fl});
                    }
                }

                auto* nextIdx = builder_->CreateAdd(idx, one, "cln_next");
                builder_->CreateStore(nextIdx, idxPtr);
                builder_->CreateBr(condBB);

                builder_->SetInsertPoint(endBB);
                freeFuncName = "lucis_vec_free_raw";
            } else {
                freeFuncName = "lucis_vec_free_raw";
            }
        } else {
            freeFuncName = "lucis_vec_free_" + suffix;
        }
    } else if (info.typeInfo->extendedKind == "Map") {
        freeFuncName = "lucis_map_free_" + info.typeInfo->builtinSuffix;
    } else if (info.typeInfo->extendedKind == "Set") {
        auto suffix = info.typeInfo->elementType
                          ? (info.typeInfo->elementType->builtinSuffix.empty()
                                 ? "raw" : info.typeInfo->elementType->builtinSuffix)
                          : info.typeInfo->builtinSuffix;
        freeFuncName = "lucis_set_free_" + suffix;
    } else {
        return;
    }

    auto callee = declareBuiltin(freeFuncName, voidTy, {ptrTy});
    builder_->CreateCall(callee, {info.alloca});
}



llvm::Value* IRGen::buildVecValueFromArrayLiteral(
    LucisParser::ArrayLitExprContext* arrLit,
    const TypeInfo* vecType,
    const std::string& tempNameHint) {
    auto* vecTy = getOrCreateVecStructType();
    if (!arrLit || !vecType || vecType->kind != TypeKind::Extended ||
        vecType->extendedKind != "Vec" || !vecType->elementType) {
        return llvm::UndefValue::get(vecTy);
    }

    auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
    auto* alloca  = builder_->CreateAlloca(vecTy, nullptr, tempNameHint);
    auto suffix   = getVecSuffix(vecType->elementType);
    auto* elemLLTy = vecType->elementType->toLLVMType(
        *context_, module_->getDataLayout());

    std::vector<llvm::Value*> vals;
    for (auto* e : arrLit->expression()) {
        vals.push_back(castValue(visit(e)));
        consumeExprIfOwnedLocal(e);
    }

    if (vals.empty()) {
        auto initFn = declareBuiltin(
            "lucis_vec_init_" + suffix,
            llvm::Type::getVoidTy(*context_),
            { ptrTy });
        builder_->CreateCall(initFn, { alloca });
        return builder_->CreateLoad(vecTy, alloca, tempNameHint + "_val");
    }

    if (suffix == "raw") {
        auto& dl       = module_->getDataLayout();
        auto  elemSz   = dl.getTypeAllocSize(elemLLTy);
        auto* elemSzVal = llvm::ConstantInt::get(usizeTy, elemSz);
        auto initCapFn = declareBuiltin(
            "lucis_vec_init_cap_raw",
            llvm::Type::getVoidTy(*context_),
            { ptrTy, usizeTy, usizeTy });
        builder_->CreateCall(initCapFn, {
            alloca,
            llvm::ConstantInt::get(usizeTy, vals.size()),
            elemSzVal
        });

        auto pushFn = declareBuiltin(
            "lucis_vec_push_raw",
            llvm::Type::getVoidTy(*context_),
            { ptrTy, ptrTy, usizeTy });
        for (auto* v : vals) {
            auto* tmp = builder_->CreateAlloca(elemLLTy);
            builder_->CreateStore(v, tmp);
            builder_->CreateCall(pushFn, { alloca, tmp, elemSzVal });
        }
    } else {
        auto initCapFn = declareBuiltin(
            "lucis_vec_init_cap_" + suffix,
            llvm::Type::getVoidTy(*context_),
            { ptrTy, usizeTy });
        builder_->CreateCall(initCapFn, {
            alloca,
            llvm::ConstantInt::get(usizeTy, vals.size())
        });

        auto pushFn = declareBuiltin(
            "lucis_vec_push_" + suffix,
            llvm::Type::getVoidTy(*context_),
            { ptrTy, elemLLTy });
        for (auto* v : vals) {
            if (v->getType() != elemLLTy) {
                if (v->getType()->isIntegerTy() && elemLLTy->isIntegerTy())
                    v = builder_->CreateIntCast(v, elemLLTy, vecType->elementType->isSigned);
                else if (v->getType()->isFloatingPointTy() && elemLLTy->isFloatingPointTy())
                    v = builder_->CreateFPCast(v, elemLLTy);
            }
            builder_->CreateCall(pushFn, { alloca, v });
        }
    }

    return builder_->CreateLoad(vecTy, alloca, tempNameHint + "_val");
}



void IRGen::emitBlockExitCleanups(const std::unordered_map<std::string, VarInfo>& savedLocals) {
    // If the current block already ends with a terminator (e.g. `ret` from a nested
    // if/else), do not append cleanup calls after it — that breaks LLVM IR and can
    // surface as invalid blocks (e.g. "Terminator found in the middle of a basic block").
    if (auto* bb = builder_->GetInsertBlock(); bb && bb->getTerminator()) return;
    for (const auto& [name, info] : locals_) {
        if (savedLocals.find(name) != savedLocals.end()) continue;
        emitCleanupForLocal(name, info);
    }
}



void IRGen::emitAllCleanups(const std::string& skipVar) {
    emitDeferredCleanups();
    emitAutoCleanups(skipVar);
}



bool IRGen::isDropTrackedLocal(const VarInfo& info) const {
    if (!info.typeInfo || info.arrayDims > 0) return false;
    if (info.typeInfo->kind == TypeKind::String && info.isBorrowed) return false;
    if (info.typeInfo->dropTracked && !info.isBorrowed) return true;
    return info.typeInfo->kind == TypeKind::String ||
           info.typeInfo->kind == TypeKind::Extended;
}



bool IRGen::isBorrowedStringValueExpr(LucisParser::ExpressionContext* expr) const {
    if (!expr) return false;
    if (isBorrowedStringExpr(expr)) return true;
    if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto it = locals_.find(ident->IDENTIFIER()->getText());
        if (it == locals_.end()) return false;
        if (!it->second.typeInfo || it->second.typeInfo->kind != TypeKind::String) return false;
        return it->second.isBorrowed;
    }
    return false;
}



void IRGen::consumeLocalByName(const std::string& name) {
    auto it = locals_.find(name);
    if (it == locals_.end()) return;
    it->second.consumed = true;
    if (isDropTrackedLocal(it->second)) {
        auto* nullVal = llvm::Constant::getNullValue(it->second.alloca->getAllocatedType());
        builder_->CreateStore(nullVal, it->second.alloca);
        return;
    }
    // Struct with owned string fields: null out string fields only
    if (it->second.typeInfo && it->second.typeInfo->kind == TypeKind::Struct) {
        auto* structLLTy = it->second.typeInfo->toLLVMType(*context_, module_->getDataLayout());
        for (size_t fieldIdx = 0; fieldIdx < it->second.typeInfo->fields.size(); fieldIdx++) {
            auto& field = it->second.typeInfo->fields[fieldIdx];
            if (field.typeInfo && field.typeInfo->kind == TypeKind::String && field.arrayDims == 0) {
                auto* fieldPtr = builder_->CreateStructGEP(structLLTy, it->second.alloca, fieldIdx, field.name + "_gep");
                auto* fieldTy = field.typeInfo->toLLVMType(*context_, module_->getDataLayout());
                auto* nullField = llvm::Constant::getNullValue(fieldTy);
                builder_->CreateStore(nullField, fieldPtr);
            }
        }
    }
}

// True when a struct field of this type owns heap data that must be
// transferred on assignment (string buffers, Vec/Map/Set, or structs
// containing such fields).


bool IRGen::isOwningFieldType(const TypeInfo* ti) {
    if (!ti) return false;
    if (ti->kind == TypeKind::String || ti->kind == TypeKind::Extended)
        return true;
    if (ti->kind == TypeKind::Struct) {
        for (auto& f : ti->fields) {
            if (f.typeInfo && isOwningFieldType(f.typeInfo))
                return true;
        }
    }
    return false;
}



void IRGen::consumeExprIfOwnedLocal(LucisParser::ExpressionContext* expr) {
    if (!expr) return;
    if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        consumeLocalByName(ident->IDENTIFIER()->getText());
        return;
    }
    // Tuple literal: (a, b, ...) — consume owned elements recursively so
    // auto-cleanup does not free strings that are part of the return value.
    if (auto* tup = dynamic_cast<LucisParser::TupleLitExprContext*>(expr)) {
        for (auto* elem : tup->expression())
            consumeExprIfOwnedLocal(elem);
        return;
    }
    // Field access: token.lexeme → null out the string field in the source struct
    if (auto* fa = dynamic_cast<LucisParser::FieldAccessExprContext*>(expr)) {
        auto fieldName = fa->IDENTIFIER()->getText();
        auto* baseExpr = fa->expression();
        if (auto* baseIdent = dynamic_cast<LucisParser::IdentExprContext*>(baseExpr)) {
            auto it = locals_.find(baseIdent->IDENTIFIER()->getText());
            if (it != locals_.end() && it->second.typeInfo &&
                it->second.typeInfo->kind == TypeKind::Struct) {
                auto* structLLTy = it->second.typeInfo->toLLVMType(*context_, module_->getDataLayout());
                for (size_t fieldIdx = 0; fieldIdx < it->second.typeInfo->fields.size(); fieldIdx++) {
                    auto& field = it->second.typeInfo->fields[fieldIdx];
                    if (field.name == fieldName && field.typeInfo &&
                        field.typeInfo->kind == TypeKind::String && field.arrayDims == 0) {
                        auto* fieldPtr = builder_->CreateStructGEP(structLLTy, it->second.alloca,
                            fieldIdx, fieldName + "_gep");
                        auto* nullFieldTy = field.typeInfo->toLLVMType(*context_, module_->getDataLayout());
                        builder_->CreateStore(llvm::Constant::getNullValue(nullFieldTy), fieldPtr);
                        return;
                    }
                }
            }
        }
    }
}


