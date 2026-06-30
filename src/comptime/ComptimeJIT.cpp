#include "comptime/ComptimeJIT.h"
#include "comptime/ComptimeIRGen.h"

// Include generated parser — LucisParser is an ANTLR class, not a namespace
#include "generated/LucisParser.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>

#ifdef LLVM_VERSION_15_OR_NEWER
#  include <llvm/TargetParser/Host.h>
#else
#  include <llvm/Support/Host.h>
#endif

#include <iostream>

struct ComptimeJIT::Impl {
    llvm::LLVMContext context;
    std::unique_ptr<llvm::ExecutionEngine> engine;
};

ComptimeJIT::ComptimeJIT() : impl_(std::make_unique<Impl>()) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    ready_ = true;
}

ComptimeJIT::~ComptimeJIT() = default;

std::unique_ptr<llvm::Module>
ComptimeJIT::compileToIR(void* funcDecl) {
    auto* func = static_cast<LucisParser::FunctionDeclContext*>(funcDecl);
    auto module = std::make_unique<llvm::Module>("comptime_jit", impl_->context);
    module->setTargetTriple(llvm::Triple(llvm::sys::getDefaultTargetTriple()));

    ComptimeIRGen irGen(impl_->context, *module);
    auto* fn = irGen.compile(func);
    if (!fn) return nullptr;

    // Verify module
    std::string err;
    llvm::raw_string_ostream os(err);
    if (llvm::verifyModule(*module, &os)) {
        std::cerr << "[comptime] IR verification failed: " << os.str() << "\n";
        return nullptr;
    }

    return module;
}

ComptimeValue
ComptimeJIT::runJIT(llvm::Module* module,
                     const std::string& funcName,
                     const std::vector<ComptimeValue>& args) {
    if (!module) return ComptimeValue::voidVal();

    // Inspect return type from the module BEFORE transferring ownership to the engine
    auto* irFn = module->getFunction(funcName);
    if (!irFn) {
        std::cerr << "[comptime] function '" << funcName << "' not found in module\n";
        return ComptimeValue::voidVal();
    }
    auto* retTy = irFn->getReturnType();
    bool isBoolRet = retTy->isIntegerTy(1);
    bool isIntRet = retTy->isIntegerTy(32);

    std::string err;
    auto clone = llvm::CloneModule(*module);
    llvm::EngineBuilder builder(std::move(clone));
    builder.setErrorStr(&err);
    builder.setEngineKind(llvm::EngineKind::JIT);
    impl_->engine.reset(builder.create());
    if (!impl_->engine) {
        std::cerr << "[comptime] failed to create JIT engine: " << err << "\n";
        return ComptimeValue::voidVal();
    }

    auto fnAddr = impl_->engine->getFunctionAddress(funcName);
    if (!fnAddr) {
        std::cerr << "[comptime] function '" << funcName << "' not found in JIT\n";
        return ComptimeValue::voidVal();
    }

    // Convert args to int32 for calling
    auto toInt32 = [](const ComptimeValue& v) -> int32_t {
        if (v.kind() == ComptimeValue::Kind::Int) return static_cast<int32_t>(v.asInt());
        if (v.kind() == ComptimeValue::Kind::Bool) return v.asBool() ? 1 : 0;
        return 0;
    };

    auto retBool = [&](auto fn) {
        return ComptimeValue::boolVal(fn() != 0);
    };
    auto retInt = [&](auto fn) {
        return ComptimeValue::intVal(fn());
    };

    if (isBoolRet) {
        if (args.size() == 0) {
            using Fn = bool (*)();
            return retBool(reinterpret_cast<Fn>(fnAddr));
        } else if (args.size() == 1) {
            using Fn = bool (*)(int32_t);
            auto* fn = reinterpret_cast<Fn>(fnAddr);
            return ComptimeValue::boolVal(fn(toInt32(args[0])) != 0);
        } else if (args.size() == 2) {
            using Fn = bool (*)(int32_t, int32_t);
            auto* fn = reinterpret_cast<Fn>(fnAddr);
            return ComptimeValue::boolVal(fn(toInt32(args[0]), toInt32(args[1])) != 0);
        } else {
            std::cerr << "[comptime] unsupported arg count for bool fn: " << args.size() << "\n";
            return ComptimeValue::voidVal();
        }
    } else if (isIntRet) {
        if (args.size() == 0) {
            using Fn = int32_t (*)();
            return ComptimeValue::intVal(reinterpret_cast<Fn>(fnAddr)());
        } else if (args.size() == 1) {
            using Fn = int32_t (*)(int32_t);
            auto* fn = reinterpret_cast<Fn>(fnAddr);
            return ComptimeValue::intVal(fn(toInt32(args[0])));
        } else if (args.size() == 2) {
            using Fn = int32_t (*)(int32_t, int32_t);
            auto* fn = reinterpret_cast<Fn>(fnAddr);
            return ComptimeValue::intVal(fn(toInt32(args[0]), toInt32(args[1])));
        } else {
            std::cerr << "[comptime] unsupported arg count: " << args.size() << "\n";
            return ComptimeValue::voidVal();
        }
    } else {
        std::cerr << "[comptime] unsupported return type in JIT runner\n";
        return ComptimeValue::voidVal();
    }
}

ComptimeValue
ComptimeJIT::evaluate(void* funcDecl,
                       const std::vector<ComptimeValue>& args) {
    auto* func = static_cast<LucisParser::FunctionDeclContext*>(funcDecl);
    auto module = compileToIR(funcDecl);
    if (!module) return ComptimeValue::voidVal();

    auto funcName = func->IDENTIFIER(0)->getText();
    return runJIT(module.get(), funcName, args);
}
