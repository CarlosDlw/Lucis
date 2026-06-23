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

    // Lookup function address and call via function pointer
    auto fnAddr = impl_->engine->getFunctionAddress(funcName);
    if (!fnAddr) {
        std::cerr << "[comptime] function '" << funcName << "' not found in JIT\n";
        return ComptimeValue::voidVal();
    }

    // Cast to native function pointer based on arg count
    // Currently only support 0-2 int32 args returning int32
    if (args.size() == 0) {
        using Fn = int32_t (*)();
        auto* fn = reinterpret_cast<Fn>(fnAddr);
        return ComptimeValue::intVal(fn());
    } else if (args.size() == 1) {
        using Fn = int32_t (*)(int32_t);
        auto* fn = reinterpret_cast<Fn>(fnAddr);
        return ComptimeValue::intVal(fn(static_cast<int32_t>(args[0].asInt())));
    } else if (args.size() == 2) {
        using Fn = int32_t (*)(int32_t, int32_t);
        auto* fn = reinterpret_cast<Fn>(fnAddr);
        return ComptimeValue::intVal(fn(
            static_cast<int32_t>(args[0].asInt()),
            static_cast<int32_t>(args[1].asInt())));
    } else {
        std::cerr << "[comptime] unsupported arg count: " << args.size() << "\n";
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
