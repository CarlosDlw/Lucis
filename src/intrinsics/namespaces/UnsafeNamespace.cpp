#include "intrinsics/IntrinsicRegistry.h"
#include "types/TypeInfo.h"

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/IRBuilder.h>

void registerUnsafeNamespace(IntrinsicRegistry& reg) {
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
        fn.returnType = "*void";
        fn.description =
            "Allocates a variadic argument list cursor and returns an opaque "
            "pointer to it.\n"
            "The returned `*void` must be passed to `va_start`, `va_arg_*`, "
            "and `va_end`.\n\n"
            "```lux\n"
            "let args = lux::unsafe::va_list();\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](llvm::IRBuilder<>& builder,
                                 llvm::Module* module,
                                 llvm::LLVMContext& context,
                                 const TypeRegistry& typeRegistry,
                                 const std::vector<llvm::Value*>& args,
                                 const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {
            // Allocate a pointer-sized buffer to hold the VA cursor
            auto* ptrTy = llvm::PointerType::get(context, 0);
            auto* vaList = builder.CreateAlloca(ptrTy, nullptr, "va_list");
            // Initialize to null
            auto* nullVal = llvm::ConstantPointerNull::get(ptrTy);
            builder.CreateStore(nullVal, vaList);
            return vaList;
        };

        unsafe.functions.push_back(std::move(fn));
    }

    // ── va_start(va, addr) ─────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "va_start";
        fn.returnType = "void";
        fn.params.push_back({"*void", false});
        fn.params.push_back({"*void", false});
        fn.description =
            "Initializes a variadic argument list cursor from a starting address.\n"
            "Stores `addr` into the cursor pointed to by `va`.\n"
            "The cursor is then advanced by each `va_arg_*` call.\n\n"
            "```lux\n"
            "lux::unsafe::va_start(args, addr);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](llvm::IRBuilder<>& builder,
                                 llvm::Module* module,
                                 llvm::LLVMContext& context,
                                 const TypeRegistry& typeRegistry,
                                 const std::vector<llvm::Value*>& args,
                                 const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {
            // args[0] = va (pointer to buffer)
            // args[1] = addr (starting address)
            builder.CreateStore(args[1], args[0]);
            return llvm::UndefValue::get(llvm::Type::getInt32Ty(context));
        };

        unsafe.functions.push_back(std::move(fn));
    }

    // ── va_arg_int32(va) ───────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "va_arg_int32";
        fn.returnType = "int32";
        fn.params.push_back({"*void", false});
        fn.description =
            "Reads the next `int32` value from the variadic argument cursor "
            "and advances the cursor past it.\n\n"
            "```lux\n"
            "int32 val = lux::unsafe::va_arg_int32(args);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](llvm::IRBuilder<>& builder,
                                 llvm::Module* module,
                                 llvm::LLVMContext& context,
                                 const TypeRegistry& typeRegistry,
                                 const std::vector<llvm::Value*>& args,
                                 const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {
            auto* ptrTy = llvm::PointerType::get(context, 0);
            auto* i32Ty = llvm::Type::getInt32Ty(context);

            // Load current cursor position from va buffer
            auto* cursor = builder.CreateLoad(ptrTy, args[0], "cursor");
            // Read int32 from cursor
            auto* val = builder.CreateLoad(i32Ty, cursor, "va_arg");
            // Advance cursor by 4 bytes
            auto* next = builder.CreateGEP(
                llvm::Type::getInt8Ty(context), cursor,
                llvm::ConstantInt::get(i32Ty, 4), "next");
            // Store advanced cursor back
            builder.CreateStore(next, args[0]);
            return val;
        };

        unsafe.functions.push_back(std::move(fn));
    }

    // ── va_arg_int64(va) ───────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "va_arg_int64";
        fn.returnType = "int64";
        fn.params.push_back({"*void", false});
        fn.description =
            "Reads the next `int64` value from the variadic argument cursor "
            "and advances the cursor past it.\n\n"
            "```lux\n"
            "int64 val = lux::unsafe::va_arg_int64(args);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](llvm::IRBuilder<>& builder,
                                 llvm::Module* module,
                                 llvm::LLVMContext& context,
                                 const TypeRegistry& typeRegistry,
                                 const std::vector<llvm::Value*>& args,
                                 const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {
            auto* ptrTy = llvm::PointerType::get(context, 0);
            auto* i64Ty = llvm::Type::getInt64Ty(context);

            auto* cursor = builder.CreateLoad(ptrTy, args[0], "cursor");
            auto* val = builder.CreateLoad(i64Ty, cursor, "va_arg");
            auto* next = builder.CreateGEP(
                llvm::Type::getInt8Ty(context), cursor,
                llvm::ConstantInt::get(i64Ty, 8), "next");
            builder.CreateStore(next, args[0]);
            return val;
        };

        unsafe.functions.push_back(std::move(fn));
    }

    // ── va_arg_float32(va) ─────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "va_arg_float32";
        fn.returnType = "float32";
        fn.params.push_back({"*void", false});
        fn.description =
            "Reads the next `float32` value from the variadic argument cursor "
            "and advances the cursor past it.\n\n"
            "```lux\n"
            "float32 val = lux::unsafe::va_arg_float32(args);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](llvm::IRBuilder<>& builder,
                                 llvm::Module* module,
                                 llvm::LLVMContext& context,
                                 const TypeRegistry& typeRegistry,
                                 const std::vector<llvm::Value*>& args,
                                 const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {
            auto* ptrTy = llvm::PointerType::get(context, 0);
            auto* floatTy = llvm::Type::getFloatTy(context);

            auto* cursor = builder.CreateLoad(ptrTy, args[0], "cursor");
            auto* val = builder.CreateLoad(floatTy, cursor, "va_arg");
            auto* next = builder.CreateGEP(
                llvm::Type::getInt8Ty(context), cursor,
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 4), "next");
            builder.CreateStore(next, args[0]);
            return val;
        };

        unsafe.functions.push_back(std::move(fn));
    }

    // ── va_arg_float64(va) ─────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "va_arg_float64";
        fn.returnType = "float64";
        fn.params.push_back({"*void", false});
        fn.description =
            "Reads the next `float64` value from the variadic argument cursor "
            "and advances the cursor past it.\n\n"
            "```lux\n"
            "float64 val = lux::unsafe::va_arg_float64(args);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](llvm::IRBuilder<>& builder,
                                 llvm::Module* module,
                                 llvm::LLVMContext& context,
                                 const TypeRegistry& typeRegistry,
                                 const std::vector<llvm::Value*>& args,
                                 const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {
            auto* ptrTy = llvm::PointerType::get(context, 0);
            auto* doubleTy = llvm::Type::getDoubleTy(context);

            auto* cursor = builder.CreateLoad(ptrTy, args[0], "cursor");
            auto* val = builder.CreateLoad(doubleTy, cursor, "va_arg");
            auto* next = builder.CreateGEP(
                llvm::Type::getInt8Ty(context), cursor,
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 8), "next");
            builder.CreateStore(next, args[0]);
            return val;
        };

        unsafe.functions.push_back(std::move(fn));
    }

    // ── va_arg_ptr(va) ─────────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "va_arg_ptr";
        fn.returnType = "*void";
        fn.params.push_back({"*void", false});
        fn.description =
            "Reads the next pointer value from the variadic argument cursor "
            "and advances the cursor past it.\n\n"
            "```lux\n"
            "let ptr: *void = lux::unsafe::va_arg_ptr(args);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [](llvm::IRBuilder<>& builder,
                                 llvm::Module* module,
                                 llvm::LLVMContext& context,
                                 const TypeRegistry& typeRegistry,
                                 const std::vector<llvm::Value*>& args,
                                 const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {
            auto* ptrTy = llvm::PointerType::get(context, 0);

            auto* cursor = builder.CreateLoad(ptrTy, args[0], "cursor");
            auto* val = builder.CreateLoad(ptrTy, cursor, "va_arg");
            auto* next = builder.CreateGEP(
                llvm::Type::getInt8Ty(context), cursor,
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context),
                                       module->getDataLayout().getPointerSize()),
                "next");
            builder.CreateStore(next, args[0]);
            return val;
        };

        unsafe.functions.push_back(std::move(fn));
    }

    // ── va_end(va) ─────────────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "va_end";
        fn.returnType = "void";
        fn.params.push_back({"*void", false});
        fn.description =
            "Invalidates a variadic argument list cursor by setting it to null.\n"
            "After this call, the cursor must not be used again without a "
            "prior `va_start`.\n\n"
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
            auto* ptrTy = llvm::PointerType::get(context, 0);
            auto* nullVal = llvm::ConstantPointerNull::get(ptrTy);
            builder.CreateStore(nullVal, args[0]);
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
        fn.params.push_back({"*void", false});
        fn.description =
            "Reads the next value of type T from the variadic argument cursor and advances the cursor.\n\n"
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
            auto* vaArg = args[0];
            auto* ptrTy = llvm::PointerType::get(context, 0);

            // Get the LLVM type for T
            auto* valueTI = typeArgs[0];
            auto& dl = module->getDataLayout();
            auto* valueTy = valueTI->toLLVMType(context, dl);
            auto valueSize = dl.getTypeAllocSize(valueTy);

            // Load cursor from va buffer
            auto* cursor = builder.CreateLoad(ptrTy, vaArg, "va_cursor");
            // Load value of type T from cursor
            auto* val = builder.CreateLoad(valueTy, cursor, "va_arg");
            // Advance cursor by the size of T
            auto* next = builder.CreateGEP(
                llvm::Type::getInt8Ty(context), cursor,
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context),
                                       valueSize), "va_next");
            // Store advanced cursor back
            builder.CreateStore(next, vaArg);

            return val;
        };

        unsafe.functions.push_back(std::move(fn));
    }

    reg.registerNamespace(std::move(unsafe));
}
