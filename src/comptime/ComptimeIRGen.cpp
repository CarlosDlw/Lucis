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
        if (name == "bool")                      return llvm::Type::getInt1Ty(ctx);
        if (name == "float32")                   return llvm::Type::getFloatTy(ctx);
        if (name == "float64")                   return llvm::Type::getDoubleTy(ctx);
        if (name == "void")                      return llvm::Type::getVoidTy(ctx);
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

    if (func->block()) {
        for (auto* stmt : func->block()->statement()) {
            // return expr;
            if (auto* ret = stmt->returnStmt()) {
                if (ret->expression()) {
                    auto* val = visitExpr(ret->expression());
                    if (val) builder_.CreateRet(val);
                } else {
                    builder_.CreateRetVoid();
                }
                break;
            }
        }
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
    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return visitExpr(paren->expression());

    return nullptr;
}

