#pragma once

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include "generated/LucisParser.h"

#include <string>
#include <unordered_map>

class ComptimeIRGen {
public:
    ComptimeIRGen(llvm::LLVMContext& ctx, llvm::Module& mod);

    llvm::Function* compile(void* funcPtr);

private:
    llvm::LLVMContext& ctx_;
    llvm::Module& mod_;
    llvm::IRBuilder<> builder_;
    std::unordered_map<std::string, llvm::Value*> locals_;

    llvm::Value* visitStmt(LucisParser::StatementContext* stmt);
    llvm::Value* visitIfStmt(LucisParser::IfStmtContext* ifStmt);
    llvm::Value* visitExpr(LucisParser::ExpressionContext* expr);
};
