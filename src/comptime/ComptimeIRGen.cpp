#include "comptime/ComptimeIRGen.h"
#include "generated/LucisParser.h"
#include <llvm/IR/Verifier.h>

#include <iostream>
#include <cstdlib>

ComptimeIRGen::ComptimeIRGen(llvm::LLVMContext& ctx, llvm::Module& mod)
    : ctx_(ctx), mod_(mod), builder_(ctx) {}

static llvm::Type* resolveType(llvm::LLVMContext& ctx,
                                LucisParser::TypeSpecContext* ts) {
    if (!ts) return llvm::Type::getInt32Ty(ctx);
    if (ts->AUTO()) return llvm::Type::getInt32Ty(ctx);
    if (ts->IDENTIFIER()) {
        auto name = ts->IDENTIFIER()->getText();
        if (name == "int8" || name == "int8_t")    return llvm::Type::getInt8Ty(ctx);
        if (name == "int16" || name == "int16_t")  return llvm::Type::getInt16Ty(ctx);
        if (name == "int32" || name == "int")       return llvm::Type::getInt32Ty(ctx);
        if (name == "int64" || name == "int64_t")  return llvm::Type::getInt64Ty(ctx);
        if (name == "uint8"  || name == "byte")    return llvm::Type::getInt8Ty(ctx);
        if (name == "uint16")                       return llvm::Type::getInt16Ty(ctx);
        if (name == "uint32")                       return llvm::Type::getInt32Ty(ctx);
        if (name == "uint64")                       return llvm::Type::getInt64Ty(ctx);
        if (name == "float32" || name == "float")   return llvm::Type::getFloatTy(ctx);
        if (name == "float64" || name == "double")  return llvm::Type::getDoubleTy(ctx);
        if (name == "char")                         return llvm::Type::getInt8Ty(ctx);
        if (name == "bool")                         return llvm::Type::getInt1Ty(ctx);
        if (name == "void")                         return llvm::Type::getVoidTy(ctx);
        return llvm::Type::getInt32Ty(ctx);
    }
    if (auto* pt = ts->primitiveType()) {
        if (pt->BOOL())    return llvm::Type::getInt1Ty(ctx);
        if (pt->CHAR())    return llvm::Type::getInt8Ty(ctx);
        if (pt->VOID())    return llvm::Type::getVoidTy(ctx);
        if (pt->INT8())  return llvm::Type::getInt8Ty(ctx);
        if (pt->INT16()) return llvm::Type::getInt16Ty(ctx);
        if (pt->INT32()) return llvm::Type::getInt32Ty(ctx);
        if (pt->INT64()) return llvm::Type::getInt64Ty(ctx);
        if (pt->INT128()) return llvm::Type::getInt128Ty(ctx);
        if (pt->ISIZE())  return llvm::Type::getInt64Ty(ctx);
        if (pt->UINT8())  return llvm::Type::getInt8Ty(ctx);
        if (pt->UINT16()) return llvm::Type::getInt16Ty(ctx);
        if (pt->UINT32()) return llvm::Type::getInt32Ty(ctx);
        if (pt->UINT64()) return llvm::Type::getInt64Ty(ctx);
        if (pt->UINT128()) return llvm::Type::getInt128Ty(ctx);
        if (pt->USIZE())  return llvm::Type::getInt64Ty(ctx);
        if (pt->FLOAT32()) return llvm::Type::getFloatTy(ctx);
        if (pt->FLOAT64()) return llvm::Type::getDoubleTy(ctx);
        if (pt->FLOAT80()) return llvm::Type::getX86_FP80Ty(ctx);
        if (pt->DOUBLE())   return llvm::Type::getDoubleTy(ctx);
        return llvm::Type::getInt32Ty(ctx);
    }
    return llvm::Type::getInt32Ty(ctx);
}

llvm::Function* ComptimeIRGen::compile(void* funcPtr) {
    auto* func = static_cast<LucisParser::FunctionDeclContext*>(funcPtr);
    auto name = func->IDENTIFIER(0)->getText();
    auto* retTy = resolveType(ctx_, func->typeSpec());

    std::vector<llvm::Type*> paramTys;
    if (auto* pl = func->paramList()) {
        for (auto* p : pl->param())
            paramTys.push_back(resolveType(ctx_, p->typeSpec()));
    }

    auto* fnTy = llvm::FunctionType::get(retTy, paramTys, false);
    auto* fn = llvm::Function::Create(
        fnTy, llvm::Function::ExternalLinkage, name, &mod_);

    auto* entry = llvm::BasicBlock::Create(ctx_, "entry", fn);
    builder_.SetInsertPoint(entry);

    locals_.clear();
    if (auto* pl = func->paramList()) {
        size_t i = 0;
        for (auto* p : pl->param()) {
            locals_[p->IDENTIFIER()->getText()] =
                TypedValue{fn->getArg(i), false};
            i++;
        }
    }

    llvm::Value* result = nullptr;
    if (func->block()) {
        for (auto* stmt : func->block()->statement()) {
            result = visitStmt(stmt);
            auto* block = builder_.GetInsertBlock();
            if (block && block->getTerminator())
                break;
        }
    }

    auto* block = builder_.GetInsertBlock();
    if (!block || !block->getTerminator()) {
        if (result && !retTy->isVoidTy())
            builder_.CreateRet(result);
        else
            builder_.CreateRetVoid();
    }

    std::string err;
    llvm::raw_string_ostream os(err);
    if (llvm::verifyFunction(*fn, &os)) {
        std::cerr << "[comptime-ir] " << os.str() << "\n";
        fn->eraseFromParent();
        return nullptr;
    }
    return fn;
}

llvm::Value* ComptimeIRGen::visitStmt(LucisParser::StatementContext* stmt) {
    if (!stmt) return nullptr;

    if (auto* ret = stmt->returnStmt()) {
        if (ret->expression())
            return visitExpr(ret->expression());
        return nullptr;
    }

    if (auto* ifSt = stmt->ifStmt())
        return visitIfStmt(ifSt);

    if (auto* nb = stmt->nakedBlockStmt()) {
        llvm::Value* val = nullptr;
        for (auto* s : nb->statement()) {
            val = visitStmt(s);
            auto* b = builder_.GetInsertBlock();
            if (b && b->getTerminator()) break;
        }
        return val;
    }

    return nullptr;
}

static bool isFloatType(llvm::Type* ty) {
    return ty->isFloatTy() || ty->isDoubleTy() || ty->isX86_FP80Ty();
}

llvm::Value* ComptimeIRGen::visitIfStmt(LucisParser::IfStmtContext* ifStmt) {
    auto* cond = visitExpr(ifStmt->expression());
    if (!cond) return nullptr;

    if (!cond->getType()->isIntegerTy(1)) {
        if (isFloatType(cond->getType()))
            cond = builder_.CreateFCmpONE(cond,
                llvm::ConstantFP::get(cond->getType(), 0.0), "ifcond");
        else
            cond = builder_.CreateICmpNE(cond,
                llvm::ConstantInt::get(cond->getType(), 0), "ifcond");
    }

    auto* fn = builder_.GetInsertBlock()->getParent();
    auto* thenBB = llvm::BasicBlock::Create(ctx_, "then", fn);
    llvm::BasicBlock* elseBB = nullptr;
    auto* mergeBB = llvm::BasicBlock::Create(ctx_, "ifmerge", fn);

    // Process else-if clauses: chain them with conditional branches
    // We start with the else block; if there are else-if clauses,
    // they get nested via the else block.
    bool hasElseOrElseIf = ifStmt->elseClause() || !ifStmt->elseIfClause().empty();
    if (hasElseOrElseIf)
        elseBB = llvm::BasicBlock::Create(ctx_, "else", fn);

    if (elseBB)
        builder_.CreateCondBr(cond, thenBB, elseBB);
    else
        builder_.CreateCondBr(cond, thenBB, mergeBB);

    // Then branch
    builder_.SetInsertPoint(thenBB);
    llvm::Value* thenVal = nullptr;
    if (auto* body = ifStmt->ifBody()) {
        if (auto* block = body->block()) {
            for (auto* s : block->statement()) {
                thenVal = visitStmt(s);
                if (builder_.GetInsertBlock()->getTerminator()) break;
            }
        } else if (auto* s = body->statement()) {
            thenVal = visitStmt(s);
        }
    }
    if (!builder_.GetInsertBlock()->getTerminator())
        builder_.CreateBr(mergeBB);
    auto* thenFinal = builder_.GetInsertBlock();

    // For else-if chains, we process them recursively.
    // We create a subgraph: else block chains through else-if blocks.
    // For simplicity with the current structure, handle else-if by
    // compiling each else-if as nested conditionals within the else block.
    llvm::Value* elseVal = nullptr;
    auto* elseFinal = elseBB;

    if (elseBB) {
        builder_.SetInsertPoint(elseBB);

        // Process else-if clauses
        auto eifClauses = ifStmt->elseIfClause();
        for (size_t i = 0; i < eifClauses.size(); i++) {
            auto* eif = eifClauses[i];
            auto* eifCond = visitExpr(eif->expression());
            if (eifCond && !eifCond->getType()->isIntegerTy(1)) {
                if (isFloatType(eifCond->getType()))
                    eifCond = builder_.CreateFCmpONE(eifCond,
                        llvm::ConstantFP::get(eifCond->getType(), 0.0), "eifcond");
                else
                    eifCond = builder_.CreateICmpNE(eifCond,
                        llvm::ConstantInt::get(eifCond->getType(), 0), "eifcond");
            }
            auto* eifThenBB = llvm::BasicBlock::Create(ctx_, "eif.then", fn);
            auto* eifNextBB = (i + 1 < eifClauses.size() || ifStmt->elseClause())
                ? llvm::BasicBlock::Create(ctx_, "eif.next", fn)
                : mergeBB;
            builder_.CreateCondBr(eifCond, eifThenBB, eifNextBB);
            builder_.SetInsertPoint(eifThenBB);
            llvm::Value* eifVal = nullptr;
            if (auto* body = eif->ifBody()) {
                if (auto* block = body->block()) {
                    for (auto* s : block->statement()) {
                        eifVal = visitStmt(s);
                        if (builder_.GetInsertBlock()->getTerminator()) break;
                    }
                } else if (auto* s = body->statement()) {
                    eifVal = visitStmt(s);
                }
            }
            if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(mergeBB);
            if (!elseVal) elseVal = eifVal;
            builder_.SetInsertPoint(eifNextBB);
            if (i + 1 < eifClauses.size() || ifStmt->elseClause())
                elseFinal = eifNextBB;
        }

        // Process else clause (plain else, no condition)
        if (auto* elseClause = ifStmt->elseClause()) {
            if (auto* body = elseClause->ifBody()) {
                if (auto* block = body->block()) {
                    for (auto* s : block->statement()) {
                        elseVal = visitStmt(s);
                        if (builder_.GetInsertBlock()->getTerminator()) break;
                    }
                } else if (auto* s = body->statement()) {
                    elseVal = visitStmt(s);
                }
            }
        }

        if (!builder_.GetInsertBlock()->getTerminator())
            builder_.CreateBr(mergeBB);
        elseFinal = builder_.GetInsertBlock();
    }

    builder_.SetInsertPoint(mergeBB);

    bool thenReaches = !thenFinal->getTerminator() ||
                       !llvm::dyn_cast<llvm::ReturnInst>(thenFinal->getTerminator());
    bool elseReaches = elseFinal && (!elseFinal->getTerminator() ||
                       !llvm::dyn_cast<llvm::ReturnInst>(elseFinal->getTerminator()));

    if (thenReaches && elseReaches && thenVal && elseVal &&
        thenVal->getType() == elseVal->getType()) {
        auto* phi = builder_.CreatePHI(thenVal->getType(), 2, "ifresult");
        phi->addIncoming(thenVal, thenFinal);
        phi->addIncoming(elseVal, elseFinal);
        return phi;
    }

    if (thenReaches && thenVal)
        return thenVal;
    if (elseReaches && elseVal)
        return elseVal;

    return thenVal ? thenVal : elseVal;
}

llvm::Value* ComptimeIRGen::visitExpr(LucisParser::ExpressionContext* expr) {
    if (!expr) return nullptr;

    if (auto* il = dynamic_cast<LucisParser::IntLitExprContext*>(expr)) {
        int64_t v = std::stoll(il->INT_LIT()->getText());
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), v, true);
    }
    if (auto* fl = dynamic_cast<LucisParser::FloatLitExprContext*>(expr)) {
        double v = std::stod(fl->FLOAT_LIT()->getText());
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), v);
    }
    if (auto* sf = dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(expr)) {
        auto text = sf->SUFFIXED_FLOAT()->getText();
        double v = std::stod(text);
        llvm::Type* ty = llvm::Type::getDoubleTy(ctx_);
        if (text.find("f32") != std::string::npos)
            ty = llvm::Type::getFloatTy(ctx_);
        return llvm::ConstantFP::get(ty, v);
    }
    if (auto* si = dynamic_cast<LucisParser::SuffixedIntFloatExprContext*>(expr)) {
        auto text = si->SUFFIXED_INT_FLOAT()->getText();
        double v = std::stod(text);
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_), v);
    }
    if (auto* si = dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(expr)) {
        auto text = si->SUFFIXED_INT()->getText();
        bool isUnsigned = false;
        unsigned bits = 32;
        auto us = text.find_first_of("ui");
        if (us != std::string::npos) {
            isUnsigned = text[us] == 'u';
            std::string suffix = text.substr(us + 1);
            if (suffix == "8") bits = 8;
            else if (suffix == "16") bits = 16;
            else if (suffix == "32") bits = 32;
            else if (suffix == "64") bits = 64;
            else if (suffix == "128") bits = 128;
            else if (suffix == "inf") bits = 256;
            else if (suffix == "size") bits = 64;
            text = text.substr(0, us);
        }
        int64_t v = std::stoll(text);
        return llvm::ConstantInt::get(
            llvm::Type::getIntNTy(ctx_, bits), v, !isUnsigned);
    }
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto it = locals_.find(id->IDENTIFIER()->getText());
        if (it != locals_.end()) return it->second.val;
        return nullptr;
    }
    if (auto* add = dynamic_cast<LucisParser::AddSubExprContext*>(expr)) {
        auto* lhs = visitExpr(add->expression(0));
        auto* rhs = visitExpr(add->expression(1));
        if (!lhs || !rhs) return nullptr;
        if (isFloatType(lhs->getType()))
            return add->PLUS() ? builder_.CreateFAdd(lhs, rhs, "add")
                               : builder_.CreateFSub(lhs, rhs, "sub");
        return add->PLUS() ? builder_.CreateAdd(lhs, rhs, "add")
                           : builder_.CreateSub(lhs, rhs, "sub");
    }
    if (auto* mul = dynamic_cast<LucisParser::MulExprContext*>(expr)) {
        auto* lhs = visitExpr(mul->expression(0));
        auto* rhs = visitExpr(mul->expression(1));
        if (!lhs || !rhs) return nullptr;
        if (isFloatType(lhs->getType()))
            return mul->STAR() ? builder_.CreateFMul(lhs, rhs, "mul")
                               : builder_.CreateFDiv(lhs, rhs, "div");
        return mul->STAR() ? builder_.CreateMul(lhs, rhs, "mul")
                           : builder_.CreateSDiv(lhs, rhs, "div");
    }
    if (auto* rel = dynamic_cast<LucisParser::RelExprContext*>(expr)) {
        auto* lhs = visitExpr(rel->expression(0));
        auto* rhs = visitExpr(rel->expression(1));
        if (!lhs || !rhs) return nullptr;
        if (isFloatType(lhs->getType())) {
            if (rel->LT())  return builder_.CreateFCmpOLT(lhs, rhs, "cmp");
            if (rel->GT())  return builder_.CreateFCmpOGT(lhs, rhs, "cmp");
            if (rel->LTE()) return builder_.CreateFCmpOLE(lhs, rhs, "cmp");
            if (rel->GTE()) return builder_.CreateFCmpOGE(lhs, rhs, "cmp");
        }
        if (rel->LT())  return builder_.CreateICmpSLT(lhs, rhs, "cmp");
        if (rel->GT())  return builder_.CreateICmpSGT(lhs, rhs, "cmp");
        if (rel->LTE()) return builder_.CreateICmpSLE(lhs, rhs, "cmp");
        if (rel->GTE()) return builder_.CreateICmpSGE(lhs, rhs, "cmp");
    }
    if (auto* eq = dynamic_cast<LucisParser::EqExprContext*>(expr)) {
        auto* lhs = visitExpr(eq->expression(0));
        auto* rhs = visitExpr(eq->expression(1));
        if (!lhs || !rhs) return nullptr;
        if (isFloatType(lhs->getType()))
            return eq->EQ() ? builder_.CreateFCmpOEQ(lhs, rhs, "cmp")
                            : builder_.CreateFCmpONE(lhs, rhs, "cmp");
        return eq->EQ() ? builder_.CreateICmpEQ(lhs, rhs, "cmp")
                        : builder_.CreateICmpNE(lhs, rhs, "cmp");
    }
    if (auto* bl = dynamic_cast<LucisParser::BoolLitExprContext*>(expr)) {
        bool v = bl->BOOL_LIT()->getText() == "true";
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx_), v);
    }
    if (auto* neg = dynamic_cast<LucisParser::NegExprContext*>(expr)) {
        auto* val = visitExpr(neg->expression());
        if (!val) return nullptr;
        if (isFloatType(val->getType()))
            return builder_.CreateFNeg(val, "neg");
        return builder_.CreateNeg(val, "neg");
    }
    if (auto* cast = dynamic_cast<LucisParser::CastExprContext*>(expr)) {
        auto* src = visitExpr(cast->expression());
        if (!src) return nullptr;
        auto* dstTy = resolveType(ctx_, cast->typeSpec());
        if (!dstTy) return nullptr;
        auto* srcTy = src->getType();
        if (srcTy == dstTy) return src;
        if (srcTy->isIntegerTy() && dstTy->isIntegerTy()) {
            unsigned srcBits = srcTy->getIntegerBitWidth();
            unsigned dstBits = dstTy->getIntegerBitWidth();
            if (srcBits < dstBits)
                return builder_.CreateSExt(src, dstTy, "cast");
            if (srcBits > dstBits)
                return builder_.CreateTrunc(src, dstTy, "cast");
            return src;
        }
        if (srcTy->isIntegerTy() && isFloatType(dstTy))
            return builder_.CreateSIToFP(src, dstTy, "cast");
        if (isFloatType(srcTy) && dstTy->isIntegerTy())
            return builder_.CreateFPToSI(src, dstTy, "cast");
        if (isFloatType(srcTy) && isFloatType(dstTy))
            return builder_.CreateFPCast(src, dstTy, "cast");
        return builder_.CreateBitCast(src, dstTy, "cast");
    }
    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return visitExpr(paren->expression());

    return nullptr;
}
