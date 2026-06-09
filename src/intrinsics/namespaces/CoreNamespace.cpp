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

    reg.registerNamespace(std::move(core));
}
