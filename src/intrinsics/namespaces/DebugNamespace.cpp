#include "intrinsics/IntrinsicRegistry.h"

void registerDebugNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg) {
    IntrinsicNamespace debug;
    debug.name = "debug";
    debug.description =
        "Debugging intrinsics for breakpoints and program introspection.\n"
        "Always available without any `use` declaration.";

    // Placeholder for future intrinsics:
    //   - breakpoint() -> @llvm.debugtrap

    reg.registerNamespace(std::move(debug));
}
