#include "comptime/ComptimeIRGen.h"
#include "generated/LucisParser.h"
#include <llvm/IR/Verifier.h>

#include <iostream>

ComptimeIRGen::ComptimeIRGen(llvm::LLVMContext& ctx, llvm::Module& mod)
    : ctx_(ctx), mod_(mod), builder_(ctx) {}

static llvm::Type* resolveType(llvm::LLVMContext& ctx,
                                LucisParser::TypeSpecContext* ts) {
    if (!ts) return llvm::Type::getInt32Ty(ctx);
    if (ts->AUTO()) return llvm::Type::getInt32Ty(ctx);
    if (ts->IDENTIFIER()) {
        auto name = ts->IDENTIFIER()->getText();
        if (name == "int32" || name == "int")   return llvm::Type::getInt32Ty(ctx);
        if (name == "int64")                     return llvm::Type::getInt64Ty(ctx);
        if (name == "int8")                      return llvm::Type::getInt8Ty(ctx);
        if (name == "float32")                   return llvm::Type::getFloatTy(ctx);
        if (name == "float64")                   return llvm::Type::getDoubleTy(ctx);
        if (name == "void")                      return llvm::Type::getVoidTy(ctx);
        if (name == "bool")                      return llvm::Type::getInt1Ty(ctx);
        if (name == "char")                      return llvm::Type::getInt8Ty(ctx);
        return llvm::Type::getInt32Ty(ctx);
    }
    if (auto* pt = ts->primitiveType()) {
        if (pt->BOOL())  return llvm::Type::getInt1Ty(ctx);
        if (pt->INT32()) return llvm::Type::getInt32Ty(ctx);
        if (pt->INT64()) return llvm::Type::getInt64Ty(ctx);
        if (pt->FLOAT32()) return llvm::Type::getFloatTy(ctx);
        if (pt->FLOAT64()) return llvm::Type::getDoubleTy(ctx);
        if (pt->VOID())  return llvm::Type::getVoidTy(ctx);
        if (pt->CHAR())  return llvm::Type::getInt8Ty(ctx);
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
            locals_[p->IDENTIFIER()->getText()] = fn->getArg(i);
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

llvm::Value* ComptimeIRGen::visitIfStmt(LucisParser::IfStmtContext* ifStmt) {
    auto* cond = visitExpr(ifStmt->expression());
    if (!cond) return nullptr;

    if (!cond->getType()->isIntegerTy(1)) {
        cond = builder_.CreateICmpNE(cond,
            llvm::ConstantInt::get(cond->getType(), 0), "ifcond");
    }

    auto* fn = builder_.GetInsertBlock()->getParent();
    auto* thenBB = llvm::BasicBlock::Create(ctx_, "then", fn);
    auto* elseBB = llvm::BasicBlock::Create(ctx_, "else", fn);
    auto* mergeBB = llvm::BasicBlock::Create(ctx_, "ifmerge", fn);

    builder_.CreateCondBr(cond, thenBB, elseBB);

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

    // Else branch
    builder_.SetInsertPoint(elseBB);
    llvm::Value* elseVal = nullptr;
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
    auto* elseFinal = builder_.GetInsertBlock();

    builder_.SetInsertPoint(mergeBB);

    bool thenReaches = !thenFinal->getTerminator() ||
                       !llvm::dyn_cast<llvm::ReturnInst>(thenFinal->getTerminator());
    bool elseReaches = !elseFinal->getTerminator() ||
                       !llvm::dyn_cast<llvm::ReturnInst>(elseFinal->getTerminator());

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
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto it = locals_.find(id->IDENTIFIER()->getText());
        if (it != locals_.end()) return it->second;
        return nullptr;
    }
    if (auto* add = dynamic_cast<LucisParser::AddSubExprContext*>(expr)) {
        auto* lhs = visitExpr(add->expression(0));
        auto* rhs = visitExpr(add->expression(1));
        if (!lhs || !rhs) return nullptr;
        if (add->PLUS())  return builder_.CreateAdd(lhs, rhs, "add");
        if (add->MINUS()) return builder_.CreateSub(lhs, rhs, "sub");
    }
    if (auto* mul = dynamic_cast<LucisParser::MulExprContext*>(expr)) {
        auto* lhs = visitExpr(mul->expression(0));
        auto* rhs = visitExpr(mul->expression(1));
        if (!lhs || !rhs) return nullptr;
        if (mul->STAR())  return builder_.CreateMul(lhs, rhs, "mul");
        if (mul->SLASH()) return builder_.CreateSDiv(lhs, rhs, "div");
    }
    if (auto* rel = dynamic_cast<LucisParser::RelExprContext*>(expr)) {
        auto* lhs = visitExpr(rel->expression(0));
        auto* rhs = visitExpr(rel->expression(1));
        if (!lhs || !rhs) return nullptr;
        if (rel->LT())  return builder_.CreateICmpSLT(lhs, rhs, "cmp");
        if (rel->GT())  return builder_.CreateICmpSGT(lhs, rhs, "cmp");
        if (rel->LTE()) return builder_.CreateICmpSLE(lhs, rhs, "cmp");
        if (rel->GTE()) return builder_.CreateICmpSGE(lhs, rhs, "cmp");
    }
    if (auto* eq = dynamic_cast<LucisParser::EqExprContext*>(expr)) {
        auto* lhs = visitExpr(eq->expression(0));
        auto* rhs = visitExpr(eq->expression(1));
        if (!lhs || !rhs) return nullptr;
        if (eq->EQ())  return builder_.CreateICmpEQ(lhs, rhs, "cmp");
        if (eq->NEQ()) return builder_.CreateICmpNE(lhs, rhs, "cmp");
    }
    if (auto* bl = dynamic_cast<LucisParser::BoolLitExprContext*>(expr)) {
        bool v = bl->BOOL_LIT()->getText() == "true";
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx_), v);
    }
    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return visitExpr(paren->expression());

    return nullptr;
}
