#include "comptime/ComptimeJIT.h"
#include "comptime/ComptimeIRGen.h"

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
#include <cstring>

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

    std::string err;
    llvm::raw_string_ostream os(err);
    if (llvm::verifyModule(*module, &os)) {
        std::cerr << "[comptime] IR verification failed: " << os.str() << "\n";
        return nullptr;
    }

    return module;
}

static bool isFloatTy(llvm::Type* ty) {
    return ty->isFloatTy() || ty->isDoubleTy();
}

static ComptimeValue retToValue(uint64_t raw, llvm::Type* ty) {
    if (ty->isIntegerTy(1))
        return ComptimeValue::boolVal(static_cast<bool>(raw & 1));
    if (ty->isIntegerTy(8) || ty->isIntegerTy(16) ||
        ty->isIntegerTy(32) || ty->isIntegerTy(64))
        return ComptimeValue::intVal(static_cast<int64_t>(raw));
    return ComptimeValue::voidVal();
}

static ComptimeValue retToValueFloat(double raw, llvm::Type* ty) {
    (void)ty;
    return ComptimeValue::floatVal(raw);
}

static uint64_t argToInt(const ComptimeValue& v) {
    if (v.kind() == ComptimeValue::Kind::Int) return static_cast<uint64_t>(v.asInt());
    if (v.kind() == ComptimeValue::Kind::Bool) return v.asBool() ? 1 : 0;
    if (v.kind() == ComptimeValue::Kind::Float) return static_cast<uint64_t>(v.asFloat());
    return 0;
}

static double argToFloat(const ComptimeValue& v) {
    if (v.kind() == ComptimeValue::Kind::Float) return v.asFloat();
    if (v.kind() == ComptimeValue::Kind::Int) return static_cast<double>(v.asInt());
    if (v.kind() == ComptimeValue::Kind::Bool) return v.asBool() ? 1.0 : 0.0;
    return 0.0;
}

// Generic integer call helper — supports 0-8 int64 args
template<typename Ret>
static ComptimeValue callIntFn(void* addr, const std::vector<ComptimeValue>& args) {
    auto toI64 = [](const ComptimeValue& v) -> int64_t {
        return static_cast<int64_t>(argToInt(v));
    };
    auto wrap = [](Ret val) -> ComptimeValue {
        if constexpr (std::is_same_v<Ret, bool>)
            return ComptimeValue::boolVal(val != 0);
        else if constexpr (std::is_same_v<Ret, float>)
            return ComptimeValue::floatVal(static_cast<double>(val));
        else if constexpr (std::is_same_v<Ret, double>)
            return ComptimeValue::floatVal(val);
        else
            return ComptimeValue::intVal(static_cast<int64_t>(val));
    };
    switch (args.size()) {
        case 0: return wrap(reinterpret_cast<Ret(*)()>(addr)());
        case 1: return wrap(reinterpret_cast<Ret(*)(int64_t)>(addr)(toI64(args[0])));
        case 2: return wrap(reinterpret_cast<Ret(*)(int64_t, int64_t)>(addr)(toI64(args[0]), toI64(args[1])));
        case 3: return wrap(reinterpret_cast<Ret(*)(int64_t, int64_t, int64_t)>(addr)(toI64(args[0]), toI64(args[1]), toI64(args[2])));
        case 4: return wrap(reinterpret_cast<Ret(*)(int64_t, int64_t, int64_t, int64_t)>(addr)(toI64(args[0]), toI64(args[1]), toI64(args[2]), toI64(args[3])));
        default:
            std::cerr << "[comptime] unsupported arg count: " << args.size() << "\n";
            return ComptimeValue::voidVal();
    }
}

// Generic float call helper — supports 0-8 double args
// Handles any return type by converting the raw double/float to ComptimeValue
template<typename Ret>
static ComptimeValue callFloatFn(void* addr, const std::vector<ComptimeValue>& args) {
    auto toD = [](const ComptimeValue& v) -> double { return argToFloat(v); };
    auto wrap = [](Ret val) -> ComptimeValue {
        if constexpr (std::is_same_v<Ret, bool>)
            return ComptimeValue::boolVal(val != 0);
        else if constexpr (std::is_same_v<Ret, float>)
            return ComptimeValue::floatVal(static_cast<double>(val));
        else if constexpr (std::is_same_v<Ret, double>)
            return ComptimeValue::floatVal(val);
        else
            return ComptimeValue::intVal(static_cast<int64_t>(val));
    };
    switch (args.size()) {
        case 0: return wrap(reinterpret_cast<Ret(*)()>(addr)());
        case 1: return wrap(reinterpret_cast<Ret(*)(double)>(addr)(toD(args[0])));
        case 2: return wrap(reinterpret_cast<Ret(*)(double, double)>(addr)(toD(args[0]), toD(args[1])));
        case 3: return wrap(reinterpret_cast<Ret(*)(double, double, double)>(addr)(toD(args[0]), toD(args[1]), toD(args[2])));
        case 4: return wrap(reinterpret_cast<Ret(*)(double, double, double, double)>(addr)(toD(args[0]), toD(args[1]), toD(args[2]), toD(args[3])));
        default:
            std::cerr << "[comptime] unsupported float arg count: " << args.size() << "\n";
            return ComptimeValue::voidVal();
    }
}

ComptimeValue
ComptimeJIT::runJIT(llvm::Module* module,
                     const std::string& funcName,
                     const std::vector<ComptimeValue>& args) {
    if (!module) return ComptimeValue::voidVal();

    auto* irFn = module->getFunction(funcName);
    if (!irFn) {
        std::cerr << "[comptime] function '" << funcName << "' not found in module\n";
        return ComptimeValue::voidVal();
    }
    auto* funcTy = irFn->getFunctionType();
    auto* retTy = funcTy->getReturnType();

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

    auto fnAddrVal = impl_->engine->getFunctionAddress(funcName);
    auto* fnAddr = reinterpret_cast<void*>(static_cast<uintptr_t>(fnAddrVal));
    if (!fnAddr) {
        std::cerr << "[comptime] function '" << funcName << "' not found in JIT\n";
        return ComptimeValue::voidVal();
    }

    // Determine whether to use float or int calling convention for params
    bool allFloat = funcTy->getNumParams() > 0;
    for (unsigned i = 0; i < funcTy->getNumParams(); i++) {
        auto* pt = funcTy->getParamType(i);
        if (!pt->isFloatTy() && !pt->isDoubleTy()) {
            allFloat = false;
            break;
        }
    }

    // Dispatch based on return type, using appropriate param convention
    if (retTy->isIntegerTy(1))
        return allFloat ? callFloatFn<bool>(fnAddr, args) : callIntFn<bool>(fnAddr, args);
    if (retTy->isIntegerTy(8))
        return allFloat ? callFloatFn<int8_t>(fnAddr, args) : callIntFn<int8_t>(fnAddr, args);
    if (retTy->isIntegerTy(16))
        return allFloat ? callFloatFn<int16_t>(fnAddr, args) : callIntFn<int16_t>(fnAddr, args);
    if (retTy->isIntegerTy(32))
        return allFloat ? callFloatFn<int32_t>(fnAddr, args) : callIntFn<int32_t>(fnAddr, args);
    if (retTy->isIntegerTy(64))
        return allFloat ? callFloatFn<int64_t>(fnAddr, args) : callIntFn<int64_t>(fnAddr, args);
    if (retTy->isFloatTy())
        return callFloatFn<float>(fnAddr, args);
    if (retTy->isDoubleTy())
        return callFloatFn<double>(fnAddr, args);
    if (retTy->isVoidTy()) {
        if (allFloat) {
            switch (args.size()) {
                case 0: reinterpret_cast<void(*)()>(fnAddr)(); break;
                case 1: reinterpret_cast<void(*)(double)>(fnAddr)(argToFloat(args[0])); break;
                case 2: reinterpret_cast<void(*)(double, double)>(fnAddr)(argToFloat(args[0]), argToFloat(args[1])); break;
                case 3: reinterpret_cast<void(*)(double, double, double)>(fnAddr)(argToFloat(args[0]), argToFloat(args[1]), argToFloat(args[2])); break;
                default:
                    std::cerr << "[comptime] unsupported void float arg count: " << args.size() << "\n";
                    return ComptimeValue::voidVal();
            }
        } else {
            switch (args.size()) {
                case 0: reinterpret_cast<void(*)()>(fnAddr)(); break;
                case 1: reinterpret_cast<void(*)(int64_t)>(fnAddr)(argToInt(args[0])); break;
                case 2: reinterpret_cast<void(*)(int64_t, int64_t)>(fnAddr)(argToInt(args[0]), argToInt(args[1])); break;
                case 3: reinterpret_cast<void(*)(int64_t, int64_t, int64_t)>(fnAddr)(argToInt(args[0]), argToInt(args[1]), argToInt(args[2])); break;
                default:
                    std::cerr << "[comptime] unsupported void int arg count: " << args.size() << "\n";
                    return ComptimeValue::voidVal();
            }
        }
        return ComptimeValue::voidVal();
    }

    std::cerr << "[comptime] unsupported return type in JIT runner\n";
    return ComptimeValue::voidVal();
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
