#include "intrinsics/IntrinsicRegistry.h"
#include "types/TypeInfo.h"
#include "types/TypeRegistry.h"

#include <functional>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Intrinsics.h>

void registerSysNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg) {

    IntrinsicNamespace sys;
    sys.name = "sys";
    sys.description =
        "Low-level system control intrinsics.\n"
        "Provides direct access to LLVM memory and codegen primitives.\n\n"
        "Always available without any `use` declaration.";

    // ── memcpy(dst, src, n) ────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "memcpy";
        fn.returnType = "void";
        fn.params.push_back({"_any", false});
        fn.params.push_back({"_any", false});
        fn.params.push_back({"_any", false});
        fn.description =
            "Copies n bytes from src memory to dst memory.\n"
            "The source and destination regions must not overlap.\n\n"
            "```lucis\n"
            "lucis::sys::memcpy(dst, src, 16);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            builder.CreateMemCpy(
                args[0], llvm::MaybeAlign(1),
                args[1], llvm::MaybeAlign(1),
                args[2], false);

            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── memmove(dst, src, n) ──────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "memmove";
        fn.returnType = "void";
        fn.params.push_back({"_any", false});
        fn.params.push_back({"_any", false});
        fn.params.push_back({"_any", false});
        fn.description =
            "Copies n bytes from src memory to dst memory.\n"
            "The source and destination regions may overlap.\n\n"
            "```lucis\n"
            "lucis::sys::memmove(dst, src, 16);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            builder.CreateMemMove(
                args[0], llvm::MaybeAlign(1),
                args[1], llvm::MaybeAlign(1),
                args[2], false);

            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── memset(dst, val, n) ──────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "memset";
        fn.returnType = "void";
        fn.params.push_back({"_any", false});
        fn.params.push_back({"_any", false});
        fn.params.push_back({"_any", false});
        fn.description =
            "Sets n bytes at dst memory to byte value val.\n\n"
            "```lucis\n"
            "lucis::sys::memset(dst, 0, 16);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* i8Ty = llvm::Type::getInt8Ty(context);
            llvm::Value* val = builder.CreateIntCast(args[1], i8Ty, false);

            builder.CreateMemSet(
                args[0], val, args[2],
                llvm::MaybeAlign(1), false);

            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── volatile_load<T>(ptr) ────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "volatile_load";
        fn.returnType = "_any";
        fn.isGeneric = true;
        fn.params.push_back({"_any", false});
        fn.description =
            "Performs a volatile load of type T from the given pointer.\n"
            "The compiler will not optimize away repeated loads.\n\n"
            "```lucis\n"
            "int32 val = lucis::sys::volatile_load<int32>(ptr);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* valTy = typeArgs[0]->toLLVMType(context, dl);
            auto* load = builder.CreateLoad(valTy, args[0], "volatile_load");
            load->setVolatile(true);
            return load;
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── volatile_store<T>(ptr, val) ──────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "volatile_store";
        fn.returnType = "void";
        fn.isGeneric = true;
        fn.params.push_back({"_any", false});
        fn.params.push_back({"_any", false});
        fn.description =
            "Performs a volatile store of a value of type T to the given pointer.\n"
            "The compiler will not optimize away the store.\n\n"
            "```lucis\n"
            "lucis::sys::volatile_store<int32>(ptr, val);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* store = builder.CreateStore(args[1], args[0]);
            store->setVolatile(true);
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── Bit manipulation ops (LLVM intrinsics) ────────────────────
    auto makeBitOp = [](const std::string& name,
                         llvm::Intrinsic::ID id,
                         const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "_any";
        fn.isGeneric = true;
        fn.params.push_back({"_any", false});
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [id, name](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* intTy = typeArgs[0]->toLLVMType(context, dl);
            auto* val = builder.CreateIntCast(args[0], intTy, false, "cast");
            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, id, {intTy});
            return builder.CreateCall(callee, {val}, name.c_str());
        };
        return fn;
    };

    sys.functions.push_back(makeBitOp("bitreverse",
        llvm::Intrinsic::bitreverse,
        "Reverses the bit pattern of an integer value.\n"
        "Every bit position is mirrored: bit 0 ↔ bit N-1.\n\n"
        "```lucis\n"
        "int32 rev = lucis::sys::bitreverse<int32>(x);\n"
        "```"));

    sys.functions.push_back(makeBitOp("bswap",
        llvm::Intrinsic::bswap,
        "Reverses the byte order of an integer value.\n"
        "The value must have a byte-swapped form (16, 32, 64, or 128 bits).\n\n"
        "```lucis\n"
        "int32 swapped = lucis::sys::bswap<int32>(x);\n"
        "```"));

    sys.functions.push_back(makeBitOp("ctpop",
        llvm::Intrinsic::ctpop,
        "Counts the number of set (1) bits in an integer value.\n"
        "Also known as population count.\n\n"
        "```lucis\n"
        "int32 ones = lucis::sys::ctpop<int32>(x);\n"
        "```"));

    // ctlz/cttz need a second arg (is_zero_undef = false → defined at zero)
    auto makeBitOpWithBool = [](const std::string& name,
                                 llvm::Intrinsic::ID id,
                                 const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "_any";
        fn.isGeneric = true;
        fn.params.push_back({"_any", false});
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [id, name](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* intTy = typeArgs[0]->toLLVMType(context, dl);
            auto* val = builder.CreateIntCast(args[0], intTy, false, "cast");
            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, id, {intTy});
            auto* isZeroUndef = llvm::ConstantInt::getFalse(context);
            return builder.CreateCall(callee, {val, isZeroUndef}, name.c_str());
        };
        return fn;
    };

    sys.functions.push_back(makeBitOpWithBool("ctlz",
        llvm::Intrinsic::ctlz,
        "Counts the number of leading zero bits in an integer value.\n"
        "If the value is zero, the result is the bit width of the type.\n\n"
        "```lucis\n"
        "int32 lz = lucis::sys::ctlz<int32>(x);\n"
        "```"));

    sys.functions.push_back(makeBitOpWithBool("cttz",
        llvm::Intrinsic::cttz,
        "Counts the number of trailing zero bits in an integer value.\n"
        "If the value is zero, the result is the bit width of the type.\n\n"
        "```lucis\n"
        "int32 tz = lucis::sys::cttz<int32>(x);\n"
        "```"));

    // ── Arithmetic with overflow (LLVM intrinsics) ───────────────
    auto makeOverflowOp = [](const std::string& name,
                              llvm::Intrinsic::ID id,
                              const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "bool";
        fn.isGeneric = true;
        fn.params.push_back({"_any", false});
        fn.params.push_back({"_any", false});
        fn.params.push_back({"_any", false});
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [id, name](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* intTy = typeArgs[0]->toLLVMType(context, dl);
            auto* a = builder.CreateIntCast(args[0], intTy, false, "a");
            auto* b = builder.CreateIntCast(args[1], intTy, false, "b");
            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, id, {intTy});
            auto* res = builder.CreateCall(callee, {a, b}, name.c_str());
            auto* val = builder.CreateExtractValue(res, 0, "val");
            auto* ov = builder.CreateExtractValue(res, 1, "ov");
            builder.CreateStore(val, args[2]);
            return ov;
        };
        return fn;
    };

    sys.functions.push_back(makeOverflowOp("sadd_with_overflow",
        llvm::Intrinsic::sadd_with_overflow,
        "Signed integer addition with overflow detection.\n"
        "Returns true if the result overflowed.\n"
        "The result is written through the third argument.\n\n"
        "```lucis\n"
        "int32 result;\n"
        "bool overflow = lucis::sys::sadd_with_overflow<int32>(a, b, &result);\n"
        "```"));

    sys.functions.push_back(makeOverflowOp("uadd_with_overflow",
        llvm::Intrinsic::uadd_with_overflow,
        "Unsigned integer addition with overflow detection.\n"
        "Returns true if the result overflowed.\n"
        "The result is written through the third argument.\n\n"
        "```lucis\n"
        "uint32 result;\n"
        "bool overflow = lucis::sys::uadd_with_overflow<uint32>(a, b, &result);\n"
        "```"));

    sys.functions.push_back(makeOverflowOp("ssub_with_overflow",
        llvm::Intrinsic::ssub_with_overflow,
        "Signed integer subtraction with overflow detection.\n"
        "Returns true if the result overflowed.\n"
        "The result is written through the third argument.\n\n"
        "```lucis\n"
        "int32 result;\n"
        "bool overflow = lucis::sys::ssub_with_overflow<int32>(a, b, &result);\n"
        "```"));

    sys.functions.push_back(makeOverflowOp("usub_with_overflow",
        llvm::Intrinsic::usub_with_overflow,
        "Unsigned integer subtraction with overflow detection.\n"
        "Returns true if the result overflowed.\n"
        "The result is written through the third argument.\n\n"
        "```lucis\n"
        "uint32 result;\n"
        "bool overflow = lucis::sys::usub_with_overflow<uint32>(a, b, &result);\n"
        "```"));

    sys.functions.push_back(makeOverflowOp("smul_with_overflow",
        llvm::Intrinsic::smul_with_overflow,
        "Signed integer multiplication with overflow detection.\n"
        "Returns true if the result overflowed.\n"
        "The result is written through the third argument.\n\n"
        "```lucis\n"
        "int32 result;\n"
        "bool overflow = lucis::sys::smul_with_overflow<int32>(a, b, &result);\n"
        "```"));

    sys.functions.push_back(makeOverflowOp("umul_with_overflow",
        llvm::Intrinsic::umul_with_overflow,
        "Unsigned integer multiplication with overflow detection.\n"
        "Returns true if the result overflowed.\n"
        "The result is written through the third argument.\n\n"
        "```lucis\n"
        "uint32 result;\n"
        "bool overflow = lucis::sys::umul_with_overflow<uint32>(a, b, &result);\n"
        "```"));

    // ── Saturating arithmetic (LLVM intrinsics) ──────────────────
    auto makeSaturatingOp = [](const std::string& name,
                                llvm::Intrinsic::ID id,
                                const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "_any";
        fn.isGeneric = true;
        fn.params.push_back({"_any", false});
        fn.params.push_back({"_any", false});
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [id, name](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* intTy = typeArgs[0]->toLLVMType(context, dl);
            auto* a = builder.CreateIntCast(args[0], intTy, true, "a");
            auto* b = builder.CreateIntCast(args[1], intTy, true, "b");
            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, id, {intTy});
            return builder.CreateCall(callee, {a, b}, name.c_str());
        };
        return fn;
    };

    sys.functions.push_back(makeSaturatingOp("sadd_sat",
        llvm::Intrinsic::sadd_sat,
        "Signed integer addition with saturation.\n"
        "Clamps the result to the range of T on overflow.\n\n"
        "```lucis\n"
        "int32 r = lucis::sys::sadd_sat<int32>(INT32_MAX, 1);  // → INT32_MAX\n"
        "```"));

    sys.functions.push_back(makeSaturatingOp("uadd_sat",
        llvm::Intrinsic::uadd_sat,
        "Unsigned integer addition with saturation.\n"
        "Clamps the result to the maximum of T on overflow.\n\n"
        "```lucis\n"
        "uint32 r = lucis::sys::uadd_sat<uint32>(0xFFFFFFFF, 1);  // → 0xFFFFFFFF\n"
        "```"));

    sys.functions.push_back(makeSaturatingOp("ssub_sat",
        llvm::Intrinsic::ssub_sat,
        "Signed integer subtraction with saturation.\n"
        "Clamps the result to the range of T on underflow.\n\n"
        "```lucis\n"
        "int32 r = lucis::sys::ssub_sat<int32>(INT32_MIN, 1);  // → INT32_MIN\n"
        "```"));

    sys.functions.push_back(makeSaturatingOp("usub_sat",
        llvm::Intrinsic::usub_sat,
        "Unsigned integer subtraction with saturation.\n"
        "Clamps the result to 0 on underflow.\n\n"
        "```lucis\n"
        "uint32 r = lucis::sys::usub_sat<uint32>(0, 1);  // → 0\n"
        "```"));

    // ── Stack / frame intrinsics ────────────────────────────────
    // frame_address(level) -> usize
    {
        IntrinsicFunction fn;
        fn.name = "frame_address";
        fn.returnType = "usize";
        fn.params.push_back({"int32", false});
        fn.description =
            "Returns the frame pointer address at the given stack level.\n"
            "Level 0 is the current function's frame.\n\n"
            "```lucis\n"
            "usize fp = lucis::sys::frame_address(0);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* ptrTy = llvm::PointerType::getUnqual(context);
            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::frameaddress, {ptrTy});
            auto* ptr = builder.CreateCall(callee, {args[0]}, "fp");
            auto& dl = module->getDataLayout();
            return builder.CreatePtrToInt(ptr, dl.getIntPtrType(context), "fp_int");
        };

        sys.functions.push_back(std::move(fn));
    }

    // return_address(level) -> usize
    {
        IntrinsicFunction fn;
        fn.name = "return_address";
        fn.returnType = "usize";
        fn.params.push_back({"int32", false});
        fn.description =
            "Returns the return address at the given stack level.\n"
            "Level 0 is the current function's return address.\n\n"
            "```lucis\n"
            "usize ra = lucis::sys::return_address(0);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            // returnaddress must NOT be overloaded (unlike frameaddress/stacksave)
            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::returnaddress, {});
            auto* ptr = builder.CreateCall(callee, {args[0]}, "ra");
            auto& dl = module->getDataLayout();
            return builder.CreatePtrToInt(ptr, dl.getIntPtrType(context), "ra_int");
        };

        sys.functions.push_back(std::move(fn));
    }

    // stack_save() -> usize
    {
        IntrinsicFunction fn;
        fn.name = "stack_save";
        fn.returnType = "usize";
        fn.description =
            "Saves the current stack pointer.\n"
            "Used with stack_restore to implement alloca-free dynamic stack frames.\n\n"
            "```lucis\n"
            "usize sp = lucis::sys::stack_save();\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* ptrTy = llvm::PointerType::getUnqual(context);
            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::stacksave, {ptrTy});
            auto* ptr = builder.CreateCall(callee, {}, "sp");
            auto& dl = module->getDataLayout();
            return builder.CreatePtrToInt(ptr, dl.getIntPtrType(context), "sp_int");
        };

        sys.functions.push_back(std::move(fn));
    }

    // stack_restore(sp: usize) -> void
    {
        IntrinsicFunction fn;
        fn.name = "stack_restore";
        fn.returnType = "void";
        fn.params.push_back({"usize", false});
        fn.description =
            "Restores the stack pointer to a previously saved value.\n"
            "Used with stack_save to implement alloca-free dynamic stack frames.\n\n"
            "```lucis\n"
            "lucis::sys::stack_restore(sp);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* ptrTy = llvm::PointerType::getUnqual(context);
            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::stackrestore, {ptrTy});
            auto* ptr = builder.CreateIntToPtr(args[0], ptrTy, "sp_ptr");
            builder.CreateCall(callee, {ptr});
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── Memory fences ────────────────────────────────────────────
    auto makeFence = [](const std::string& name,
                         llvm::AtomicOrdering ordering,
                         const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "void";
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [ordering](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            builder.CreateFence(ordering);
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };
        return fn;
    };

    sys.functions.push_back(makeFence("fence_acquire",
        llvm::AtomicOrdering::Acquire,
        "Memory fence with acquire semantics.\n"
        "All loads and stores after this fence will not be reordered "
        "before it.\n\n"
        "```lucis\n"
        "lucis::sys::fence_acquire();\n"
        "```"));

    sys.functions.push_back(makeFence("fence_release",
        llvm::AtomicOrdering::Release,
        "Memory fence with release semantics.\n"
        "All loads and stores before this fence will not be reordered "
        "after it.\n\n"
        "```lucis\n"
        "lucis::sys::fence_release();\n"
        "```"));

    sys.functions.push_back(makeFence("fence_acq_rel",
        llvm::AtomicOrdering::AcquireRelease,
        "Memory fence with acquire-release semantics.\n"
        "Combines acquire and release barriers.\n\n"
        "```lucis\n"
        "lucis::sys::fence_acq_rel();\n"
        "```"));

    sys.functions.push_back(makeFence("fence_seq_cst",
        llvm::AtomicOrdering::SequentiallyConsistent,
        "Memory fence with sequentially-consistent semantics.\n"
        "The strongest fence — establishes a single total order.\n\n"
        "```lucis\n"
        "lucis::sys::fence_seq_cst();\n"
        "```"));

    // ── Prefetch intrinsics ──────────────────────────────────────
    auto makePrefetch = [](const std::string& name,
                            int rw,
                            const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "void";
        fn.params.push_back({"_any", false});
        fn.params.push_back({"int32", false});
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [rw](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* ptrTy = llvm::PointerType::getUnqual(context);
            auto* i32Ty = llvm::Type::getInt32Ty(context);
            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::prefetch, {ptrTy});
            auto* rwVal = llvm::ConstantInt::get(i32Ty, rw);
            auto* cacheVal = llvm::ConstantInt::get(i32Ty, 1);
            builder.CreateCall(callee, {args[0], rwVal, args[1], cacheVal});
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };
        return fn;
    };

    sys.functions.push_back(makePrefetch("prefetch_read", 0,
        "Prefetches data from memory for reading.\n"
        "addr must be a pointer. locality: 0 (no temporal reuse), "
        "1-3 (increasing persistence).\n\n"
        "```lucis\n"
        "lucis::sys::prefetch_read(&data, 3);\n"
        "```"));

    sys.functions.push_back(makePrefetch("prefetch_write", 1,
        "Prefetches data from memory for writing.\n"
        "addr must be a pointer. locality: 0 (no temporal reuse), "
        "1-3 (increasing persistence).\n\n"
        "```lucis\n"
        "lucis::sys::prefetch_write(&data, 3);\n"
        "```"));

    // ── CPU intrinsics ───────────────────────────────────────────

    // breakpoint() — @llvm.debugtrap (unlike trap(), continues after)
    {
        IntrinsicFunction fn;
        fn.name = "breakpoint";
        fn.returnType = "void";
        fn.description =
            "Triggers a debugger breakpoint.\n"
            "Execution pauses if a debugger is attached; otherwise "
            "the instruction may be ignored.\n"
            "Unlike lucis::core::trap(), execution continues after the breakpoint.\n\n"
            "```lucis\n"
            "lucis::sys::breakpoint();\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::debugtrap, {});
            builder.CreateCall(callee, {});
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };

        sys.functions.push_back(std::move(fn));
    }

    // read_cycle_counter() — u64
    {
        IntrinsicFunction fn;
        fn.name = "read_cycle_counter";
        fn.returnType = "u64";
        fn.description =
            "Reads the current CPU cycle counter.\n"
            "On x86_64 this is `rdtsc`, on AArch64 `mrs cntvct_el0`.\n"
            "The result is a 64-bit monotonically-increasing counter.\n\n"
            "```lucis\n"
            "u64 start = lucis::sys::read_cycle_counter();\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::readcyclecounter, {});
            return builder.CreateCall(callee, {}, "cycle");
        };

        sys.functions.push_back(std::move(fn));
    }

    // cpu_relax() — target-specific inline asm
    {
        IntrinsicFunction fn;
        fn.name = "cpu_relax";
        fn.returnType = "void";
        fn.description =
            "Hints the CPU that the current thread is in a spin-wait loop.\n"
            "On x86_64 this emits `pause`; on AArch64 `yield`.\n"
            "Improves performance and power efficiency of spinlocks.\n\n"
            "```lucis\n"
            "lucis::sys::cpu_relax();\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            const auto tripleStr = module->getTargetTriple().str();
            std::string asmStr = "nop";
            if (tripleStr.find("x86_64") == 0 || tripleStr.find("i386") == 0 || tripleStr.find("i686") == 0)
                asmStr = "pause";
            else if (tripleStr.find("aarch64") == 0 || tripleStr.find("arm64") == 0)
                asmStr = "yield";
            else if (tripleStr.find("riscv64") == 0)
                asmStr = "fence iorw, iorw";

            auto* voidTy = llvm::Type::getVoidTy(context);
            auto* ft = llvm::FunctionType::get(voidTy, false);
            auto* asmFn = llvm::InlineAsm::get(ft, asmStr, "",
                true, false, llvm::InlineAsm::AD_ATT);
            builder.CreateCall(asmFn);
            return llvm::UndefValue::get(voidTy);
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── Syscall intrinsics ───────────────────────────────────────

    auto makeSyscall = [](int numArgs) {
        std::string fnName = "syscall" + std::to_string(numArgs);
        std::string desc = "Invokes a system call with " +
            std::to_string(numArgs) + " argument" +
            (numArgs == 1 ? "" : "s") + ".\n"
            "Returns the syscall return value (rax on x86_64).\n"
            "Target-dependent: x86_64 `syscall`, AArch64 `svc #0`, "
            "RISC-V `ecall`.\n\n"
            "```lucis\n"
            "int64 ret = lucis::sys::" + fnName + "(" +
            std::to_string(numArgs == 0 ? 0 : 0) +
            (numArgs >= 1 ? ", arg1" : "") +
            (numArgs >= 2 ? ", arg2" : "") +
            (numArgs >= 3 ? ", arg3" : "") +
            (numArgs >= 4 ? ", arg4" : "") +
            (numArgs >= 5 ? ", arg5" : "") +
            (numArgs >= 6 ? ", arg6" : "") +
            ");\n"
            "```";

        IntrinsicFunction fn;
        fn.name = fnName;
        fn.returnType = "int64";
        fn.params.push_back({"int64", false});
        for (int i = 0; i < numArgs; i++)
            fn.params.push_back({"int64", false});
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [numArgs](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            const auto tripleStr = module->getTargetTriple().str();
            bool isX86 = tripleStr.find("x86_64") == 0;
            bool isAArch64 = tripleStr.find("aarch64") == 0;
            bool isRISCV = tripleStr.find("riscv64") == 0;

            auto* i64Ty = llvm::Type::getInt64Ty(context);

            if (isX86) {
                // Output: rax (result)
                // Inputs: rax=num, rdi, rsi, rdx, r10, r8, r9
                // Clobbers: rcx, r11, memory
                std::string constraints = "={rax},{rax}";
                if (numArgs >= 1) constraints += ",{rdi}";
                if (numArgs >= 2) constraints += ",{rsi}";
                if (numArgs >= 3) constraints += ",{rdx}";
                if (numArgs >= 4) constraints += ",{r10}";
                if (numArgs >= 5) constraints += ",{r8}";
                if (numArgs >= 6) constraints += ",{r9}";
                constraints += ",~{rcx},~{r11},~{memory}";

                std::vector<llvm::Type*> paramTys;
                for (int i = 0; i < numArgs + 1; i++)
                    paramTys.push_back(i64Ty);

                auto* ft = llvm::FunctionType::get(i64Ty, paramTys, false);
                auto* asmFn = llvm::InlineAsm::get(
                    ft, "syscall", constraints,
                    true, false, llvm::InlineAsm::AD_ATT);

                return builder.CreateCall(asmFn, args);
            }
            else if (isAArch64) {
                // Output: x0 (result)
                // Inputs: x8=num, x0..x5
                // Clobbers: x0-x5, x8, memory
                std::string constraints = "={x0},{x8}";
                if (numArgs >= 1) constraints += ",{x0}";
                if (numArgs >= 2) constraints += ",{x1}";
                if (numArgs >= 3) constraints += ",{x2}";
                if (numArgs >= 4) constraints += ",{x3}";
                if (numArgs >= 5) constraints += ",{x4}";
                if (numArgs >= 6) constraints += ",{x5}";
                constraints += ",~{x0},~{x1},~{x2},~{x3},~{x4},~{x5},~{x8},~{memory}";

                std::vector<llvm::Type*> paramTys;
                for (int i = 0; i < numArgs + 1; i++)
                    paramTys.push_back(i64Ty);

                auto* ft = llvm::FunctionType::get(i64Ty, paramTys, false);
                auto* asmFn = llvm::InlineAsm::get(
                    ft, "svc #0", constraints,
                    true, false, llvm::InlineAsm::AD_ATT);

                return builder.CreateCall(asmFn, args);
            }
            else if (isRISCV) {
                // Output: x10 (a0, result)
                // Inputs: x17 (a7)=num, x10..x15 (a0..a5)
                // Clobbers: x10-x15, x17, memory
                std::string constraints = "={x10},{x17}";
                if (numArgs >= 1) constraints += ",{x10}";
                if (numArgs >= 2) constraints += ",{x11}";
                if (numArgs >= 3) constraints += ",{x12}";
                if (numArgs >= 4) constraints += ",{x13}";
                if (numArgs >= 5) constraints += ",{x14}";
                if (numArgs >= 6) constraints += ",{x15}";
                constraints += ",~{x10},~{x11},~{x12},~{x13},~{x14},~{x15},~{x17},~{memory}";

                std::vector<llvm::Type*> paramTys;
                for (int i = 0; i < numArgs + 1; i++)
                    paramTys.push_back(i64Ty);

                auto* ft = llvm::FunctionType::get(i64Ty, paramTys, false);
                auto* asmFn = llvm::InlineAsm::get(
                    ft, "ecall", constraints,
                    true, false, llvm::InlineAsm::AD_ATT);

                return builder.CreateCall(asmFn, args);
            }
            else {
                llvm::report_fatal_error("syscall not supported on this target");
            }
        };

        return fn;
    };

    for (int n = 0; n <= 6; n++)
        sys.functions.push_back(makeSyscall(n));

    // ── read<T>(ptr) → T — typed load from raw pointer ─────────────
    {
        IntrinsicFunction fn;
        fn.name = "read";
        fn.returnType = "_any";
        fn.isGeneric = true;
        fn.params.push_back({"_any", false});
        fn.description =
            "Performs a typed read from a raw pointer.\n"
            "Equivalent to dereferencing a pointer of type T.\n\n"
            "```lucis\n"
            "int32 val = lucis::sys::read<int32>(ptr);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* valTy = typeArgs[0]->toLLVMType(context, dl);
            return builder.CreateLoad(valTy, args[0], "read");
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── write<T>(ptr, val) → void — typed store to raw pointer ────
    {
        IntrinsicFunction fn;
        fn.name = "write";
        fn.returnType = "void";
        fn.isGeneric = true;
        fn.params.push_back({"_any", false});
        fn.params.push_back({"_any", false});
        fn.description =
            "Performs a typed write to a raw pointer.\n"
            "Equivalent to assigning through a pointer of type T.\n\n"
            "```lucis\n"
            "lucis::sys::write<int32>(ptr, 42);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            builder.CreateStore(args[1], args[0]);
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── offset<T>(ptr, count) → *T — typed pointer arithmetic ─────
    {
        IntrinsicFunction fn;
        fn.name = "offset";
        fn.returnType = "_any";
        fn.isGeneric = true;
        fn.params.push_back({"_any", false});
        fn.params.push_back({"int64", false});
        fn.description =
            "Performs typed pointer arithmetic via GEP.\n"
            "Returns a pointer of the same type advanced by count elements.\n"
            "T must be a pointer type (e.g. <*int32>).\n\n"
            "```lucis\n"
            "*int32 next = lucis::sys::offset<*int32>(ptr, 3);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* ptrTy = typeArgs[0]->toLLVMType(context, dl);
            auto* elemTy = typeArgs[0]->pointeeType->toLLVMType(context, dl);
            (void)ptrTy;
            return builder.CreateGEP(elemTy, args[0], args[1], "offset");
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── bitcast<T, U>(val) → U — reinterpret bits ────────────────
    {
        IntrinsicFunction fn;
        fn.name = "bitcast";
        fn.returnType = "_any";
        fn.isGeneric = true;
        fn.returnTypeArgIndex = 1;
        fn.params.push_back({"_any", false});
        fn.description =
            "Reinterprets the bit pattern of a value as a different type.\n"
            "Both types must have the same size in memory.\n\n"
            "```lucis\n"
            "float32 f = lucis::sys::bitcast<int32, float32>(i);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* targetTy = typeArgs[1]->toLLVMType(context, dl);
            return builder.CreateBitCast(args[0], targetTy, "bitcast");
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── assume(cond) → void — optimizer hint ─────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "assume";
        fn.returnType = "void";
        fn.params.push_back({"bool", false});
        fn.description =
            "Provides an optimizer hint that the given condition is always true.\n"
            "If the condition is false at runtime, behavior is undefined.\n"
            "Useful for expressing invariants to enable better optimization.\n\n"
            "```lucis\n"
            "lucis::sys::assume(ptr != null);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            builder.CreateAssumption(args[0]);
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── unreachable() → void — marks code as unreachable ──────────
    {
        IntrinsicFunction fn;
        fn.name = "unreachable";
        fn.returnType = "void";
        fn.description =
            "Marks the current point in code as unreachable.\n"
            "Behavior is undefined if control flow reaches this call.\n"
            "Useful in default cases that should never execute.\n\n"
            "```lucis\n"
            "lucis::sys::unreachable();\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            builder.CreateUnreachable();
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── size_of<T>() → usize — compile-time type size ────────────
    {
        IntrinsicFunction fn;
        fn.name = "size_of";
        fn.returnType = "usize";
        fn.isGeneric = true;
        fn.description =
            "Returns the size of type T in bytes at compile time.\n"
            "The result is a constant determined from the LLVM DataLayout.\n\n"
            "```lucis\n"
            "usize sz = lucis::sys::size_of<int32>();  // → 4\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* ty = typeArgs[0]->toLLVMType(context, dl);
            uint64_t size = dl.getTypeStoreSize(ty);
            return llvm::ConstantInt::get(dl.getIntPtrType(context), size);
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── align_of<T>() → usize — compile-time type alignment ──────
    {
        IntrinsicFunction fn;
        fn.name = "align_of";
        fn.returnType = "usize";
        fn.isGeneric = true;
        fn.description =
            "Returns the alignment of type T in bytes at compile time.\n"
            "The result is a constant determined from the LLVM DataLayout.\n\n"
            "```lucis\n"
            "usize al = lucis::sys::align_of<int32>();  // → 4\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* ty = typeArgs[0]->toLLVMType(context, dl);
            uint64_t align = dl.getABITypeAlign(ty).value();
            return llvm::ConstantInt::get(dl.getIntPtrType(context), align);
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── Float math intrinsics ────────────────────────────────────
    auto makeFloatOp = [](const std::string& name,
                          const std::string& llvmName,
                          const std::string& desc,
                          unsigned minArgs = 1) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "_any";
        fn.isGeneric = true;
        for (unsigned i = 0; i < minArgs; i++)
            fn.params.push_back({"_any", false});
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [llvmName, name, minArgs](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* floatTy = typeArgs[0]->toLLVMType(context, dl);
            auto id = llvm::Intrinsic::lookupIntrinsicID(llvmName);
            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, id, {floatTy});

            std::vector<llvm::Value*> callArgs;
            for (unsigned i = 0; i < minArgs; i++)
                callArgs.push_back(args[i]);
            return builder.CreateCall(callee, callArgs, name.c_str());
        };
        return fn;
    };

    sys.functions.push_back(makeFloatOp("sqrt",
        "llvm.sqrt",
        "Returns the square root of a floating-point value.\n\n"
        "```lucis\n"
        "float64 r = lucis::sys::sqrt<float64>(2.0);  // → 1.414...\n"
        "```"));

    sys.functions.push_back(makeFloatOp("fma",
        "llvm.fma",
        "Fused multiply-add: returns a * b + c with a single rounding.\n\n"
        "```lucis\n"
        "float64 r = lucis::sys::fma<float64>(a, b, c);\n"
        "```", 3));

    sys.functions.push_back(makeFloatOp("ceil",
        "llvm.ceil",
        "Rounds a floating-point value up to the nearest integral value.\n\n"
        "```lucis\n"
        "float64 r = lucis::sys::ceil<float64>(3.14);  // → 4.0\n"
        "```"));

    sys.functions.push_back(makeFloatOp("floor",
        "llvm.floor",
        "Rounds a floating-point value down to the nearest integral value.\n\n"
        "```lucis\n"
        "float64 r = lucis::sys::floor<float64>(3.14);  // → 3.0\n"
        "```"));

    sys.functions.push_back(makeFloatOp("trunc",
        "llvm.trunc",
        "Rounds a floating-point value towards zero to the nearest integral value.\n\n"
        "```lucis\n"
        "float64 r = lucis::sys::trunc<float64>(-3.14);  // → -3.0\n"
        "```"));

    sys.functions.push_back(makeFloatOp("round",
        "llvm.round",
        "Rounds a floating-point value to the nearest integral value, "
        "rounding away from zero for halfway cases.\n\n"
        "```lucis\n"
        "float64 r = lucis::sys::round<float64>(3.5);  // → 4.0\n"
        "```"));

    sys.functions.push_back(makeFloatOp("fabs",
        "llvm.fabs",
        "Returns the absolute value of a floating-point value.\n\n"
        "```lucis\n"
        "float64 r = lucis::sys::fabs<float64>(-3.14);  // → 3.14\n"
        "```"));

    sys.functions.push_back(makeFloatOp("minimum",
        "llvm.minimum",
        "Returns the minimum of two floating-point values (propagates NaN).\n\n"
        "```lucis\n"
        "float64 r = lucis::sys::minimum<float64>(3.0, 5.0);  // → 3.0\n"
        "```", 2));

    sys.functions.push_back(makeFloatOp("maximum",
        "llvm.maximum",
        "Returns the maximum of two floating-point values (propagates NaN).\n\n"
        "```lucis\n"
        "float64 r = lucis::sys::maximum<float64>(3.0, 5.0);  // → 5.0\n"
        "```", 2));

    sys.functions.push_back(makeFloatOp("copysign",
        "llvm.copysign",
        "Returns a value with the magnitude of the first operand and "
        "the sign of the second.\n\n"
        "```lucis\n"
        "float64 r = lucis::sys::copysign<float64>(1.0, -1.0);  // → -1.0\n"
        "```", 2));

    // ── Integer abs (requires is_int_min_poison flag) ────────────
    {
        IntrinsicFunction fn;
        fn.name = "abs";
        fn.returnType = "_any";
        fn.isGeneric = true;
        fn.params.push_back({"_any", false});
        fn.description =
            "Returns the absolute value of a signed integer.\n\n"
            "```lucis\n"
            "int32 a = lucis::sys::abs<int32>(-42);  // → 42\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* intTy = typeArgs[0]->toLLVMType(context, dl);
            auto* val = builder.CreateIntCast(args[0], intTy, true, "cast");
            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::abs, {intTy});
            auto* isIntMinPoison = llvm::ConstantInt::getFalse(context);
            return builder.CreateCall(callee, {val, isIntMinPoison}, "abs");
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── Rotate left / right ──────────────────────────────────────
    auto makeRotateOp = [](const std::string& name,
                           llvm::Intrinsic::ID id,
                           const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "_any";
        fn.isGeneric = true;
        fn.params.push_back({"_any", false});
        fn.params.push_back({"_any", false});
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [id, name](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* intTy = typeArgs[0]->toLLVMType(context, dl);
            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, id, {intTy});
            auto* val = builder.CreateIntCast(args[0], intTy, false, "cast");
            auto* shift = builder.CreateIntCast(args[1], intTy, false, "shift");
            return builder.CreateCall(callee, {val, val, shift},
                                      name.c_str());
        };
        return fn;
    };

    sys.functions.push_back(makeRotateOp("rotl",
        llvm::Intrinsic::fshl,
        "Rotates the bits of an integer value left by shift positions.\n\n"
        "```lucis\n"
        "int32 r = lucis::sys::rotl<int32>(0x80000001, 1);  // → 3\n"
        "```"));

    sys.functions.push_back(makeRotateOp("rotr",
        llvm::Intrinsic::fshr,
        "Rotates the bits of an integer value right by shift positions.\n\n"
        "```lucis\n"
        "int32 r = lucis::sys::rotr<int32>(0x80000001, 1);  // → 0xC0000000\n"
        "```"));

    // ── Branch hint ──────────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "expect";
        fn.returnType = "bool";
        fn.params.push_back({"bool", false});
        fn.params.push_back({"bool", false});
        fn.description =
            "Provides a branch-weight hint to the optimizer.\n"
            "Returns the first argument unchanged. The second argument "
            "is the expected value (used by LLVM branch-weight metadata).\n\n"
            "```lucis\n"
            "if lucis::sys::expect(x > 0, true) { ... }\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::expect, {args[0]->getType()});
            llvm::Value* expected = args[1];
            // args[1] is i1 (bool), need i1 for llvm.expect
            return builder.CreateCall(callee, {args[0], expected}, "expect");
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── Lifetime hints ───────────────────────────────────────────
    auto makeLifetimeOp = [](const std::string& name,
                              llvm::Intrinsic::ID id,
                              const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "void";
        fn.params.push_back({"_any", false});   // ptr
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [id](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* ptrTy = llvm::PointerType::get(context, 0);
            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, id, {ptrTy});
            builder.CreateCall(callee, {args[0]});
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };
        return fn;
    };

    sys.functions.push_back(makeLifetimeOp("lifetime_start",
        llvm::Intrinsic::lifetime_start,
        "Marks the start of an object's lifetime for the optimizer.\n"
        "The pointer must point to the start of the allocation.\n\n"
        "```lucis\n"
        "lucis::sys::lifetime_start(&obj);\n"
        "```"));

    sys.functions.push_back(makeLifetimeOp("lifetime_end",
        llvm::Intrinsic::lifetime_end,
        "Marks the end of an object's lifetime for the optimizer.\n"
        "Accessing the object after this is undefined behavior.\n\n"
        "```lucis\n"
        "lucis::sys::lifetime_end(&obj);\n"
        "```"));

    // ── Hardware random number generator (x86-64 rdrand) ─────────
    auto makeRdRand = [](const std::string& name,
                          const std::string& asmInsn,
                          const std::string& intTypeName,
                          const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "bool";
        fn.params.push_back({"_any", false}); // ptr to write result
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [name, asmInsn, intTypeName](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            const auto tripleStr = module->getTargetTriple().str();
            auto* boolTy = llvm::Type::getInt1Ty(context);

            if (tripleStr.find("x86_64") != 0 && tripleStr.find("i386") != 0 &&
                tripleStr.find("i686") != 0) {
                // Unsupported target: store 0 and return false
                auto* intTy = llvm::Type::getIntNTy(context,
                    intTypeName == "uint16" ? 16 :
                    intTypeName == "uint32" ? 32 : 64);
                builder.CreateStore(llvm::ConstantInt::get(intTy, 0), args[0]);
                return llvm::ConstantInt::getFalse(context);
            }

            auto bits = intTypeName == "uint16" ? 16 :
                        intTypeName == "uint32" ? 32 : 64;
            auto* intTy = llvm::Type::getIntNTy(context, bits);
            auto* i8Ty  = llvm::Type::getInt8Ty(context);

            // Build function type: returns {iN, i8} (value + success byte)
            auto* structTy = llvm::StructType::get(context, {intTy, i8Ty});
            auto* ft = llvm::FunctionType::get(structTy, false);

            std::string constraints = "=r,=r,~{cc},~{dirflag},~{fpsr},~{flags}";
            auto* asmFn = llvm::InlineAsm::get(ft, asmInsn, constraints,
                true, false, llvm::InlineAsm::AD_ATT);

            auto* callResult = builder.CreateCall(asmFn, {}, name.c_str());
            auto* val = builder.CreateExtractValue(callResult, {0}, "val");
            auto* ok_byte = builder.CreateExtractValue(callResult, {1}, "ok");
            auto* ok = builder.CreateTrunc(ok_byte, boolTy);
            // Only store if success (LLVM select for safety)
            builder.CreateStore(val, args[0]);
            return ok;
        };
        return fn;
    };

    sys.functions.push_back(makeRdRand("rdrand16",
        "rdrandw $0; setc $1",
        "uint16",
        "Hardware random number generator (16-bit).\n"
        "Writes the random value through the pointer and returns true on success.\n"
        "May return false if the RNG is not ready (retry recommended).\n"
        "Available on x86-64 with RDRAND support (Ivy Bridge+).\n\n"
        "```lucis\n"
        "uint16 val = 0;\n"
        "while !lucis::sys::rdrand16(&val) {}\n"
        "```"));

    sys.functions.push_back(makeRdRand("rdrand32",
        "rdrandl $0; setc $1",
        "uint32",
        "Hardware random number generator (32-bit).\n"
        "Writes the random value through the pointer and returns true on success.\n"
        "Available on x86-64 with RDRAND support (Ivy Bridge+).\n\n"
        "```lucis\n"
        "uint32 val = 0;\n"
        "while !lucis::sys::rdrand32(&val) {}\n"
        "```"));

    sys.functions.push_back(makeRdRand("rdrand64",
        "rdrandq $0; setc $1",
        "uint64",
        "Hardware random number generator (64-bit).\n"
        "Writes the random value through the pointer and returns true on success.\n"
        "Available on x86-64 with RDRAND support (Ivy Bridge+).\n\n"
        "```lucis\n"
        "uint64 val = 0;\n"
        "while !lucis::sys::rdrand64(&val) {}\n"
        "```"));

    // ── Endianness conversion ────────────────────────────────────
    // Uses DataLayout to detect host endianness, then either applies
    // llvm.bswap or returns identity.

    auto makeEndianOp = [](const std::string& name,
                           bool bswapOnLE,
                           const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "_any";
        fn.isGeneric = true;
        fn.params.push_back({"_any", false});
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [name, bswapOnLE](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto& dl = module->getDataLayout();
            auto* intTy = typeArgs[0]->toLLVMType(context, dl);
            auto* val = builder.CreateIntCast(args[0], intTy, false, "cast");

            bool needSwap = (dl.isLittleEndian() == bswapOnLE);
            if (!needSwap) return val;

            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::bswap, {intTy});
            return builder.CreateCall(callee, {val}, name.c_str());
        };
        return fn;
    };

    sys.functions.push_back(makeEndianOp("to_be",
        true,
        "Converts an integer from host byte order to big-endian.\n"
        "On little-endian hosts this is a byte swap; on big-endian hosts a no-op.\n\n"
        "```lucis\n"
        "uint32 be = lucis::sys::to_be<uint32>(host_val);\n"
        "```"));

    sys.functions.push_back(makeEndianOp("to_le",
        false,
        "Converts an integer from host byte order to little-endian.\n"
        "On little-endian hosts this is a no-op; on big-endian hosts a byte swap.\n\n"
        "```lucis\n"
        "uint32 le = lucis::sys::to_le<uint32>(host_val);\n"
        "```"));

    sys.functions.push_back(makeEndianOp("from_be",
        true,
        "Converts a big-endian integer to host byte order.\n"
        "On little-endian hosts this is a byte swap; on big-endian hosts a no-op.\n\n"
        "```lucis\n"
        "uint32 host = lucis::sys::from_be<uint32>(be_val);\n"
        "```"));

    sys.functions.push_back(makeEndianOp("from_le",
        false,
        "Converts a little-endian integer to host byte order.\n"
        "On little-endian hosts this is a no-op; on big-endian hosts a byte swap.\n\n"
        "```lucis\n"
        "uint32 host = lucis::sys::from_le<uint32>(le_val);\n"
        "```"));

    // ── Instruction cache flush ──────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "cache_flush";
        fn.returnType = "void";
        fn.params.push_back({"_any", false}); // start
        fn.params.push_back({"_any", false}); // end
        fn.description =
            "Flushes the instruction cache for the given memory range.\n"
            "Useful for JIT compilers and self-modifying code.\n\n"
            "```lucis\n"
            "lucis::sys::cache_flush(&code_start, &code_end);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* callee = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::clear_cache);
            builder.CreateCall(callee, {args[0], args[1]});
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── TLS base access (x86-64 FSGSBASE) ────────────────────────
    auto makeFSBaseOp = [](const std::string& name,
                            bool isRead,
                            const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "usize";
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [name, isRead](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            const auto tripleStr = module->getTargetTriple().str();
            auto* usizeTy = llvm::Type::getIntNTy(context,
                module->getDataLayout().getPointerSizeInBits());

            if (tripleStr.find("x86_64") != 0) {
                // Unsupported target
                return llvm::ConstantInt::get(usizeTy, 0);
            }

            std::string asmStr = isRead ? "rdfsbase $0" : "wrfsbase $0";
            std::string constraints = isRead ? "=r,~{memory}" : "r,~{memory}";
            auto* retTy = isRead ? usizeTy : llvm::Type::getVoidTy(context);
            auto* ft = llvm::FunctionType::get(retTy,
                isRead ? llvm::ArrayRef<llvm::Type*>() :
                         llvm::ArrayRef<llvm::Type*>({usizeTy}),
                false);

            auto* asmFn = llvm::InlineAsm::get(ft, asmStr, constraints,
                true, false, llvm::InlineAsm::AD_ATT);

            if (isRead)
                return builder.CreateCall(asmFn, {}, name.c_str());
            else
                builder.CreateCall(asmFn, {args[0]});
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };
        return fn;
    };

    sys.functions.push_back(makeFSBaseOp("read_fs_base",
        true,
        "Reads the x86-64 FS segment base register (TLS pointer on Linux).\n"
        "Requires FSGSBASE CPU feature (Ivy Bridge+).\n"
        "Returns 0 on unsupported targets.\n\n"
        "```lucis\n"
        "usize tls = lucis::sys::read_fs_base();\n"
        "```"));

    // write_fs_base takes a value
    {
        IntrinsicFunction fn;
        fn.name = "write_fs_base";
        fn.returnType = "void";
        fn.params.push_back({"usize", false});
        fn.description =
            "Writes the x86-64 FS segment base register.\n"
            "Requires FSGSBASE CPU feature (Ivy Bridge+) and kernel support.\n"
            "No-op on unsupported targets.\n\n"
            "```lucis\n"
            "lucis::sys::write_fs_base(new_tls);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            const auto tripleStr = module->getTargetTriple().str();
            if (tripleStr.find("x86_64") != 0)
                return llvm::UndefValue::get(llvm::Type::getVoidTy(context));

            auto* usizeTy = args[0]->getType();
            auto* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                {usizeTy}, false);
            auto* asmFn = llvm::InlineAsm::get(ft, "wrfsbase $0",
                "r,~{memory}", true, false, llvm::InlineAsm::AD_ATT);
            builder.CreateCall(asmFn, {args[0]});
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };

        sys.functions.push_back(std::move(fn));
    }

    // ── CPUID (x86-64) ──────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "cpuid";
        fn.returnType = "void";
        fn.params.push_back({"uint32", false}); // leaf
        fn.params.push_back({"uint32", false}); // subleaf
        fn.params.push_back({"_any", false});   // &eax
        fn.params.push_back({"_any", false});   // &ebx
        fn.params.push_back({"_any", false});   // &ecx
        fn.params.push_back({"_any", false});   // &edx
        fn.description =
            "Executes the x86 CPUID instruction.\n"
            "Writes eax, ebx, ecx, edx through the given pointers.\n"
            "For basic leaves, pass 0 as subleaf.\n\n"
            "```lucis\n"
            "uint32 eax, ebx, ecx, edx;\n"
            "lucis::sys::cpuid(0, 0, &eax, &ebx, &ecx, &edx);\n"
            "// eax = max standard leaf, ebx/ecx/edx = vendor string\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            const auto tripleStr = module->getTargetTriple().str();
            auto* i32Ty = llvm::Type::getInt32Ty(context);

            if (tripleStr.find("x86_64") != 0 && tripleStr.find("i386") != 0 &&
                tripleStr.find("i686") != 0) {
                auto* zero = llvm::ConstantInt::get(i32Ty, 0);
                builder.CreateStore(zero, args[2]);
                builder.CreateStore(zero, args[3]);
                builder.CreateStore(zero, args[4]);
                builder.CreateStore(zero, args[5]);
                return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
            }

            // Build asm: returns {i32,i32,i32,i32} = {eax,ebx,ecx,edx}
            auto* structTy = llvm::StructType::get(context,
                {i32Ty, i32Ty, i32Ty, i32Ty});
            auto* ft = llvm::FunctionType::get(structTy,
                {i32Ty, i32Ty}, false);  // inputs: leaf, subleaf

            auto* asmFn = llvm::InlineAsm::get(ft,
                "cpuid",
                "={ax},={bx},={cx},={dx},0,2",
                true, false, llvm::InlineAsm::AD_ATT);

            auto* result = builder.CreateCall(asmFn,
                {args[0], args[1]}, "cpuid");
            builder.CreateStore(builder.CreateExtractValue(result, {0}), args[2]);
            builder.CreateStore(builder.CreateExtractValue(result, {1}), args[3]);
            builder.CreateStore(builder.CreateExtractValue(result, {2}), args[4]);
            builder.CreateStore(builder.CreateExtractValue(result, {3}), args[5]);
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };

        sys.functions.push_back(std::move(fn));
    }

    // ═══════════════════════════════════════════════════════════
    // Tier 3 — Platform-specific hardware access (cross-target safe)
    // ═══════════════════════════════════════════════════════════

    auto isX86 = [](const std::string& triple) {
        return triple.find("x86_64") == 0 || triple.find("i386") == 0 ||
               triple.find("i686") == 0;
    };

    // ── I/O Ports ───────────────────────────────────────────────
    auto makePortIn = [&](const std::string& name, const std::string& asmInsn,
                           unsigned bits) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = bits == 8 ? "uint8" : bits == 16 ? "uint16" : "uint32";
        fn.params.push_back({"uint16", false});
        fn.description = "Reads " + std::to_string(bits) + " bits from an x86 I/O port.\n"
            "Returns 0 on unsupported targets.\n\n"
            "```lucis\n"
            "uint" + std::to_string(bits) + " val = lucis::sys::" + name + "(0x60);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [name, asmInsn, bits](
            llvm::IRBuilder<>& builder, llvm::Module* module,
            llvm::LLVMContext& context, const TypeRegistry&,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>&) -> llvm::Value* {

            auto triple = module->getTargetTriple().str();
            auto* retTy = llvm::Type::getIntNTy(context, bits);
            if (triple.find("x86_64") != 0 && triple.find("i386") != 0 &&
                triple.find("i686") != 0)
                return llvm::ConstantInt::get(retTy, 0);

            // in{b,w,l} reads from DX port into {AL,AX,EAX}
            std::string constraints = "={ax},0,~{dirflag},~{fpsr},~{flags}";
            auto* ft = llvm::FunctionType::get(retTy, {retTy}, false);
            auto* asmFn = llvm::InlineAsm::get(ft, asmInsn, constraints,
                true, false, llvm::InlineAsm::AD_ATT);
            // Extend 16-bit port to register width for the "0" matching constraint
            auto* portVal = builder.CreateZExt(args[0], retTy);
            return builder.CreateCall(asmFn, {portVal}, name);
        };
        return fn;
    };

    auto makePortOut = [&](const std::string& name, const std::string& asmInsn,
                            unsigned bits) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "void";
        fn.params.push_back({"uint16", false});
        fn.params.push_back({bits == 8 ? "uint8" : bits == 16 ? "uint16" : "uint32", false});
        fn.description = "Writes " + std::to_string(bits) + " bits to an x86 I/O port.\n"
            "No-op on unsupported targets.\n\n"
            "```lucis\n"
            "lucis::sys::" + name + "(0x60, val);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [asmInsn, bits](
            llvm::IRBuilder<>& builder, llvm::Module* module,
            llvm::LLVMContext& context, const TypeRegistry&,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>&) -> llvm::Value* {

            auto triple = module->getTargetTriple().str();
            if (triple.find("x86_64") != 0 && triple.find("i386") != 0 &&
                triple.find("i686") != 0)
                return llvm::UndefValue::get(llvm::Type::getVoidTy(context));

            auto* retTy = llvm::Type::getIntNTy(context, bits);
            // First arg = DX port (uint16), second arg = value
            // out{b,w,l}: port in DX, value in {AL,AX,EAX}
            std::string constraints = "0,{dx},~{dirflag},~{fpsr},~{flags}";
            auto* ft = llvm::FunctionType::get(retTy,
                {retTy, llvm::Type::getInt16Ty(context)}, false);
            auto* asmFn = llvm::InlineAsm::get(ft, asmInsn, constraints,
                true, false, llvm::InlineAsm::AD_ATT);
            auto* zext = builder.CreateZExt(args[1], retTy);
            builder.CreateCall(asmFn, {zext, args[0]});
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };
        return fn;
    };

    sys.functions.push_back(makePortIn("inb",  "inb $1, $0", 8));
    sys.functions.push_back(makePortIn("inw",  "inw $1, $0", 16));
    sys.functions.push_back(makePortIn("inl",  "inl $1, $0", 32));
    sys.functions.push_back(makePortOut("outb", "outb $0, $1", 8));
    sys.functions.push_back(makePortOut("outw", "outw $0, $1", 16));
    sys.functions.push_back(makePortOut("outl", "outl $0, $1", 32));

    // ── Interrupt control ───────────────────────────────────────
    auto makeIntrOp = [&](const std::string& name, const std::string& asmInsn,
                           const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "void";
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [name, asmInsn](
            llvm::IRBuilder<>& builder, llvm::Module* module,
            llvm::LLVMContext& context, const TypeRegistry&,
            const std::vector<llvm::Value*>&,
            const std::vector<const TypeInfo*>&) -> llvm::Value* {

            auto triple = module->getTargetTriple().str();
            if (triple.find("x86_64") != 0 && triple.find("i386") != 0 &&
                triple.find("i686") != 0)
                return llvm::UndefValue::get(llvm::Type::getVoidTy(context));

            auto* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);
            auto* asmFn = llvm::InlineAsm::get(ft, asmInsn,
                "~{dirflag},~{fpsr},~{flags}", true, false,
                llvm::InlineAsm::AD_ATT);
            builder.CreateCall(asmFn);
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };
        return fn;
    };

    sys.functions.push_back(makeIntrOp("cli", "cli",
        "Disables maskable interrupts on x86 (clear interrupt flag).\n"
        "No-op on unsupported targets.\n\n"
        "```lucis\nlucis::sys::cli();\n```"));
    sys.functions.push_back(makeIntrOp("sti", "sti",
        "Enables maskable interrupts on x86 (set interrupt flag).\n"
        "No-op on unsupported targets.\n\n"
        "```lucis\nlucis::sys::sti();\n```"));

    // ── MSR access ──────────────────────────────────────────────
    auto makeMSROp = [&](const std::string& name, bool isRead,
                          const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.description = desc;

        if (isRead) {
            fn.returnType = "uint64";
            fn.params.push_back({"uint32", false}); // msr index
        } else {
            fn.returnType = "void";
            fn.params.push_back({"uint32", false}); // msr index
            fn.params.push_back({"uint64", false}); // value
        }

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [name, isRead](
            llvm::IRBuilder<>& builder, llvm::Module* module,
            llvm::LLVMContext& context, const TypeRegistry&,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>&) -> llvm::Value* {

            auto triple = module->getTargetTriple().str();
            auto* i64Ty = llvm::Type::getInt64Ty(context);
            auto* i32Ty = llvm::Type::getInt32Ty(context);

            if (triple.find("x86_64") != 0)
            return isRead ? static_cast<llvm::Value*>(llvm::ConstantInt::get(i64Ty, 0))
                          : static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getVoidTy(context)));

            // rdmsr: ecx=msr, returns edx:eax as uint64
            // wrmsr: ecx=msr, edx:eax=value
            if (isRead) {
                auto* structTy = llvm::StructType::get(context, {i64Ty});
                auto* ft = llvm::FunctionType::get(structTy, {i32Ty}, false);
                auto* asmFn = llvm::InlineAsm::get(ft,
                    "rdmsr", "=A,0,~{dirflag},~{fpsr},~{flags}",
                    true, false, llvm::InlineAsm::AD_ATT);
                auto* result = builder.CreateCall(asmFn, {args[0]}, name);
                return builder.CreateExtractValue(result, {0});
            } else {
                auto* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                    {i32Ty, i64Ty}, false);
                auto* asmFn = llvm::InlineAsm::get(ft,
                    "wrmsr", "{cx},A,~{dirflag},~{fpsr},~{flags}",
                    true, false, llvm::InlineAsm::AD_ATT);
                builder.CreateCall(asmFn, {args[0], args[1]});
                return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
            }
        };
        return fn;
    };

    sys.functions.push_back(makeMSROp("rdmsr", true,
        "Reads a 64-bit model-specific register (MSR) on x86-64.\n"
        "Returns 0 on unsupported targets.\n\n"
        "```lucis\n"
        "uint64 val = lucis::sys::rdmsr(0xC0000080);  // EFER\n"
        "```"));
    sys.functions.push_back(makeMSROp("wrmsr", false,
        "Writes a 64-bit model-specific register (MSR) on x86-64.\n"
        "No-op on unsupported targets.\n\n"
        "```lucis\n"
        "lucis::sys::wrmsr(0xC0000080, val);  // EFER\n"
        "```\n"
        "⚠ Requires kernel privileges (ring 0) for most MSRs."));

    // ── Control registers ───────────────────────────────────────
    auto makeCROp = [&](const std::string& name, const std::string& reg,
                         bool isRead, const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.description = desc;

        if (isRead) {
            fn.returnType = "usize";
        } else {
            fn.returnType = "void";
            fn.params.push_back({"usize", false});
        }

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [name, reg, isRead](
            llvm::IRBuilder<>& builder, llvm::Module* module,
            llvm::LLVMContext& context, const TypeRegistry&,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>&) -> llvm::Value* {

            auto triple = module->getTargetTriple().str();
            auto bits = module->getDataLayout().getPointerSizeInBits();
            auto* usizeTy = llvm::Type::getIntNTy(context, bits);

            if (triple.find("x86_64") != 0)
            return isRead ? static_cast<llvm::Value*>(llvm::ConstantInt::get(usizeTy, 0))
                          : static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getVoidTy(context)));

            std::string asmInsn = isRead ? ("mov %" + reg + ", $0")
                                         : ("mov $0, %" + reg);
            std::string constraints = isRead ? "=r,~{dirflag},~{fpsr},~{flags}"
                                             : "r,~{dirflag},~{fpsr},~{flags}";
            auto* retTy = isRead ? usizeTy
                                 : llvm::Type::getVoidTy(context);
            auto* ft = llvm::FunctionType::get(retTy,
                isRead ? llvm::ArrayRef<llvm::Type*>()
                       : llvm::ArrayRef<llvm::Type*>({usizeTy}),
                false);
            auto* asmFn = llvm::InlineAsm::get(ft, asmInsn, constraints,
                true, false, llvm::InlineAsm::AD_ATT);

            if (isRead)
                return builder.CreateCall(asmFn, {}, name);
            builder.CreateCall(asmFn, {args[0]});
            return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
        };
        return fn;
    };

    sys.functions.push_back(makeCROp("read_cr0", "cr0", true,
        "Reads the x86 CR0 control register.\nReturns 0 on unsupported targets.\n\n"
        "```lucis\nusize cr0 = lucis::sys::read_cr0();\n```"));
    sys.functions.push_back(makeCROp("write_cr0", "cr0", false,
        "Writes the x86 CR0 control register. No-op on unsupported targets.\n"
        "⚠ Requires kernel privileges (ring 0).\n\n"
        "```lucis\nlucis::sys::write_cr0(val);\n```"));
    sys.functions.push_back(makeCROp("read_cr2", "cr2", true,
        "Reads the x86 CR2 control register (page fault linear address).\n"
        "Returns 0 on unsupported targets.\n\n"
        "```lucis\nusize cr2 = lucis::sys::read_cr2();\n```"));
    sys.functions.push_back(makeCROp("read_cr3", "cr3", true,
        "Reads the x86 CR3 control register (page directory base).\n"
        "Returns 0 on unsupported targets.\n\n"
        "```lucis\nusize cr3 = lucis::sys::read_cr3();\n```"));
    sys.functions.push_back(makeCROp("write_cr3", "cr3", false,
        "Writes the x86 CR3 control register (page directory base).\n"
        "No-op on unsupported targets.\n"
        "⚠ Requires kernel privileges (ring 0).\n\n"
        "```lucis\nlucis::sys::write_cr3(val);\n```"));
    sys.functions.push_back(makeCROp("read_cr4", "cr4", true,
        "Reads the x86 CR4 control register.\nReturns 0 on unsupported targets.\n\n"
        "```lucis\nusize cr4 = lucis::sys::read_cr4();\n```"));
    sys.functions.push_back(makeCROp("write_cr4", "cr4", false,
        "Writes the x86 CR4 control register. No-op on unsupported targets.\n"
        "⚠ Requires kernel privileges (ring 0).\n\n"
        "```lucis\nlucis::sys::write_cr4(val);\n```"));

    // ── hlt: halt CPU until next interrupt ─────────────────
    sys.functions.push_back(makeIntrOp("hlt", "hlt",
        "Halts the CPU until the next interrupt arrives.\n"
        "⚠ Requires kernel privileges (ring 0).\n\n"
        "```lucis\nlucis::sys::hlt();\n```"));

    // ── lgdt / lidt / invlpg: table/MMU operations ─────────
    auto makeTableOp = [&](const std::string& name, const std::string& asmInsn,
                            const std::string& desc) {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = "void";
        fn.params.push_back({"_any", false});
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [name, asmInsn](
            llvm::IRBuilder<>& builder, llvm::Module* module,
            llvm::LLVMContext& context, const TypeRegistry&,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>&) -> llvm::Value* {

            auto triple = module->getTargetTriple().str();
            if (triple.find("x86_64") != 0 && triple.find("i386") != 0 &&
                triple.find("i686") != 0)
                return llvm::UndefValue::get(llvm::Type::getVoidTy(context));

            auto* voidTy = llvm::Type::getVoidTy(context);
            auto* ptrTy = llvm::PointerType::getUnqual(context);
            auto* ft = llvm::FunctionType::get(voidTy, {ptrTy}, false);
            auto* asmFn = llvm::InlineAsm::get(ft,
                asmInsn + " ($0)",
                "r,~{memory}",
                true, false, llvm::InlineAsm::AD_ATT);
            builder.CreateCall(asmFn, {args[0]});
            return llvm::UndefValue::get(voidTy);
        };
        return fn;
    };

    sys.functions.push_back(makeTableOp("lgdt", "lgdt",
        R"(Loads the Global Descriptor Table Register (GDTR) on x86-64.
⚠ Requires kernel privileges (ring 0).

```lucis
lucis::sys::lgdt(&descriptor);
```)"));

    sys.functions.push_back(makeTableOp("lidt", "lidt",
        R"(Loads the Interrupt Descriptor Table Register (IDTR) on x86-64.
⚠ Requires kernel privileges (ring 0).

```lucis
lucis::sys::lidt(&descriptor);
```)"));

    sys.functions.push_back(makeTableOp("invlpg", "invlpg",
        R"(Invalidates the TLB entry for the given virtual address on x86-64.
⚠ Requires kernel privileges (ring 0).

```lucis
lucis::sys::invlpg(&address);
```)"));

    reg.registerNamespace(std::move(sys));
}
