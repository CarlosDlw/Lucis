#include "intrinsics/IntrinsicRegistry.h"
#include "types/TypeInfo.h"
#include "types/TypeRegistry.h"

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>

void registerUnsafeNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg) {
    // Register the underlying VAList structure type
    {
        TypeInfo tag;
        tag.name = "va_list_tag";
        tag.kind = TypeKind::VAList;
        typeReg.registerType(std::move(tag));
    }

    // Register va_list as a handle (pointer to the tag)
    {
        TypeInfo ptr;
        ptr.name = "va_list";
        ptr.kind = TypeKind::Pointer;
        ptr.pointeeType = typeReg.lookup("va_list_tag");
        ptr.builtinSuffix = "ptr";
        typeReg.registerType(std::move(ptr));
    }

    IntrinsicNamespace unsafe;
    unsafe.name = "unsafe";
    unsafe.description =
        "Unsafe low-level memory and variadic argument intrinsics.\n"
        "Provides direct access to the calling convention's variadic argument "
        "area without any type safety guarantees. Use with extreme care.\n\n"
        "Always available without any `use` declaration.";

    // ── va_list() ──────────────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "va_list";
        fn.returnType = "va_list";
        fn.description =
            "Allocates a variadic argument list state handle and returns it.\n"
            "The returned `va_list` must be passed to `va_start`, `va_arg`, "
            "and `va_end`.\n\n"
            "```lux\n"
            "va_list args = lux::unsafe::va_list();\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](llvm::IRBuilder<>& builder,
                                 llvm::Module* module,
                                 llvm::LLVMContext& context,
                                 const TypeRegistry& typeRegistry,
                                 const std::vector<llvm::Value*>& args,
                                 const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {
            auto* tagTI = typeRegistry.lookup("va_list_tag");
            auto& dl = module->getDataLayout();
            auto* vaListTagTy = tagTI->toLLVMType(context, dl);
            
            // va_list() in Lux is an expression that returns a new state pointer.
            // We allocate the storage on the stack.
            return builder.CreateAlloca(vaListTagTy, nullptr, "va_list_storage");
        };

        unsafe.functions.push_back(std::move(fn));
    }

    // ── va_start(va) ─────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "va_start";
        fn.returnType = "void";
        fn.params.push_back({"va_list", false});
        fn.description =
            "Initializes a variadic argument list state handle.\n"
            "The state is then advanced by each `va_arg` call.\n\n"
            "```lux\n"
            "lux::unsafe::va_start(args);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](llvm::IRBuilder<>& builder,
                                 llvm::Module* module,
                                 llvm::LLVMContext& context,
                                 const TypeRegistry& typeRegistry,
                                 const std::vector<llvm::Value*>& args,
                                 const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {
            // args[0] = va_list handle (pointer to storage)
            auto* vaStartFunc = llvm::Intrinsic::getOrInsertDeclaration(module, llvm::Intrinsic::vastart);
            
            // llvm.va_start expects a pointer to the storage (i8*)
            auto* i8PtrTy = llvm::PointerType::getUnqual(context);
            auto* vaArg = builder.CreateBitCast(args[0], i8PtrTy);
            
            builder.CreateCall(vaStartFunc, {vaArg});
            return llvm::UndefValue::get(llvm::Type::getInt32Ty(context));
        };

        unsafe.functions.push_back(std::move(fn));
    }

    // ── va_arg<T>(va) ───────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "va_arg";
        fn.isGeneric = true;
        fn.returnType = "_any";
        fn.params.push_back({"va_list", false});
        fn.description =
            "Reads the next value of type T from the variadic argument list and advances the handle.\n\n"
            "```lux\n"
            "T val = lux::unsafe::va_arg<T>(args);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](llvm::IRBuilder<>& builder,
                                 llvm::Module* module,
                                 llvm::LLVMContext& context,
                                 const TypeRegistry& typeRegistry,
                                 const std::vector<llvm::Value*>& args,
                                 const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {
            auto* vaListPtr = args[0]; // The va_list handle (pointer to storage)
            auto* valueTI = typeArgs[0];
            auto& dl = module->getDataLayout();
            auto* valueTy = valueTI->toLLVMType(context, dl);

            // The LLVM va_arg instruction handles target-specific ABI details.
            return builder.CreateVAArg(vaListPtr, valueTy);
        };

        unsafe.functions.push_back(std::move(fn));
    }

    // ── va_end(va) ─────────────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "va_end";
        fn.returnType = "void";
        fn.params.push_back({"va_list", false});
        fn.description =
            "Invalidates a variadic argument list state handle.\n"
            "```lux\n"
            "lux::unsafe::va_end(args);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](llvm::IRBuilder<>& builder,
                                 llvm::Module* module,
                                 llvm::LLVMContext& context,
                                 const TypeRegistry& typeRegistry,
                                 const std::vector<llvm::Value*>& args,
                                 const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {
            auto* vaEndFunc = llvm::Intrinsic::getOrInsertDeclaration(module, llvm::Intrinsic::vaend);
            
            auto* i8PtrTy = llvm::PointerType::getUnqual(context);
            auto* vaArg = builder.CreateBitCast(args[0], i8PtrTy);
            
            builder.CreateCall(vaEndFunc, {vaArg});
            return llvm::UndefValue::get(llvm::Type::getInt32Ty(context));
        };

        unsafe.functions.push_back(std::move(fn));
    }

    reg.registerNamespace(std::move(unsafe));
}
