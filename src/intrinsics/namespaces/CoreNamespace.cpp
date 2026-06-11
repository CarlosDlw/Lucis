#include "intrinsics/IntrinsicRegistry.h"

void registerCoreNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg) {
    IntrinsicNamespace core;
    core.name = "core";
    core.description =
        "Core language intrinsics for program control and debugging.\n"
        "Always available without any `use` declaration.";

    // ── trap() ─────────────────────────────────────────────────────
    {
        IntrinsicFunction trap;
        trap.name = "trap";
        trap.returnType = "void";
        trap.description =
            "Aborts program execution immediately by raising a hardware trap.\n"
            "Lowers directly to `@llvm.trap` at the IR level.\n\n"
            "Unlike `std::process::abort()`, no library-level cleanup is performed. "
            "Use for diagnosing unreachable program states.";

        trap.lowering.kind = IntrinsicFunction::Lowering::LLVMIntrinsic;
        trap.lowering.intrinsicName = "llvm.trap";

        core.functions.push_back(std::move(trap));
    }

    // ── sum(...) ───────────────────────────────────────────────────
    {
        IntrinsicFunction sum;
        sum.name = "sum";
        sum.returnType = "int32";
        sum.isVariadic = true;
        sum.description =
            "Sums all variadic arguments and returns the total.\n\n"
            "```lux\n"
            "int32 total = lux::core::sum(1, 2, 3);  // 6\n"
            "```";

        sum.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        sum.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* i32Ty = llvm::Type::getInt32Ty(context);
            llvm::Value* sum = llvm::ConstantInt::get(i32Ty, 0);
            for (auto* arg : args)
                sum = builder.CreateAdd(sum, arg, "sum");
            return sum;
        };

        core.functions.push_back(std::move(sum));
    }

    // ── sum_prefix(int32 prefix, ...) ──────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "sum_prefix";
        fn.returnType = "int32";
        fn.isVariadic = true;
        fn.params.push_back({"int32", false});
        fn.description =
            "Adds a required prefix value to the sum of all variadic arguments.\n\n"
            "```lux\n"
            "int32 total = lux::core::sum_prefix(100, 1, 2, 3);  // 106\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* i32Ty = llvm::Type::getInt32Ty(context);
            // args[0] is the fixed prefix
            // args[1..] are the variadic values
            llvm::Value* sum = args[0];
            for (size_t i = 1; i < args.size(); i++)
                sum = builder.CreateAdd(sum, args[i], "sum");
            return sum;
        };

        core.functions.push_back(std::move(fn));
    }

    reg.registerNamespace(std::move(core));
}
