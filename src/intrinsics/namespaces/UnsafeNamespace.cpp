#include "intrinsics/IntrinsicRegistry.h"
#include "types/TypeInfo.h"
#include "types/TypeRegistry.h"

#include <functional>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>

void registerUnsafeNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg) {

    // Helper to build a typed va_arg intrinsic.
    // Types are created inside the lambda when the LLVMContext is available.
    auto makeTypedVaArg = [](const std::string& name,
                              const std::string& returnType,
                              std::function<llvm::Type*(llvm::LLVMContext&)> typeFn,
                              bool truncToBool = false) {
        IntrinsicFunction f;
        f.name = name;
        f.returnType = returnType;
        f.params.push_back({"va_list", false});
        f.description =
            "Reads the next " + returnType +
            " value from the variadic argument list and advances the handle.\n\n"
            "```lux\n" + returnType +
            " val = lux::unsafe::" + name + "(args);\n"
            "```";

        f.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        f.lowering.emitIR = [typeFn, truncToBool, name](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* llvmTy = typeFn(context);
            llvm::Value* val = builder.CreateVAArg(args[0], llvmTy, name.c_str());
            if (truncToBool)
                val = builder.CreateTrunc(val, llvm::Type::getInt1Ty(context), "tobool");
            return val;
        };
        return f;
    };

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
            auto* i8PtrTy = llvm::PointerType::getUnqual(context);
            auto* vaStartFunc = llvm::Intrinsic::getOrInsertDeclaration(module, llvm::Intrinsic::vastart, {i8PtrTy});

            // The va_list type in Lux is a pointer to a va_list_tag struct.
            // But LLVM's va_start expects a pointer directly to the storage area.
            // args[0] is the va_list value (pointer to the struct).
            // We need to get the i8* pointing to the underlying storage.
            // i8PtrTy declared above for the intrinsic overload
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

    // ── Typed va_arg helpers ───────────────────────────────────────
    // These use only types that LLVM 22's X86 backend can lower for
    // va_arg (i32, i64, float, double, ptr).  va_arg_bool reads i32
    // (C default promotion) then truncates to i1.

    unsafe.functions.push_back(
        makeTypedVaArg("va_arg_int32",  "int32",   [](auto& ctx){ return llvm::Type::getInt32Ty(ctx); }));
    unsafe.functions.push_back(
        makeTypedVaArg("va_arg_int64",  "int64",   [](auto& ctx){ return llvm::Type::getInt64Ty(ctx); }));
    unsafe.functions.push_back(
        makeTypedVaArg("va_arg_float32","float32", [](auto& ctx){ return llvm::Type::getFloatTy(ctx); }));
    unsafe.functions.push_back(
        makeTypedVaArg("va_arg_float64","float64", [](auto& ctx){ return llvm::Type::getDoubleTy(ctx); }));
    unsafe.functions.push_back(
        makeTypedVaArg("va_arg_ptr",    "*void",   [](auto& ctx){ return llvm::PointerType::getUnqual(ctx); }));
    unsafe.functions.push_back(
        makeTypedVaArg("va_arg_bool",   "bool",    [](auto& ctx){ return llvm::Type::getInt32Ty(ctx); }, true));

    // ── va_arg_string(va) ─────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "va_arg_string";
        fn.returnType = "string";
        fn.params.push_back({"va_list", false});
        fn.description =
            "Reads the next C string (char*) from the variadic argument list, "
            "converts it to a Lux string via strlen, and advances the handle.\n\n"
            "```lux\n"
            "string val = lux::unsafe::va_arg_string(args);\n"
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
            auto* ptrTy = llvm::PointerType::getUnqual(context);
            auto* i64Ty = llvm::Type::getInt64Ty(context);

            // Step 1: Read the char* from va_list (safe — same type as va_arg_ptr)
            llvm::Value* cstr = builder.CreateVAArg(args[0], ptrTy, "va_arg_str_ptr");

            // Step 2: Call lux_fromCStr to get { ptr, len }
            auto* retStructTy = llvm::StructType::get(context, {ptrTy, i64Ty});
            auto callee = module->getOrInsertFunction("lux_fromCStr", retStructTy, ptrTy);
            auto* retVal = builder.CreateCall(callee, {cstr}, "va_arg_str");

            // Step 3: Wrap into Lux string type { ptr, len }
            auto* strTy = typeRegistry.lookup("string")->toLLVMType(context, dl);
            llvm::Value* strStruct = llvm::UndefValue::get(strTy);
            strStruct = builder.CreateInsertValue(strStruct,
                builder.CreateExtractValue(retVal, 0, "va_arg_str_ptr"), 0, "str_ptr");
            strStruct = builder.CreateInsertValue(strStruct,
                builder.CreateExtractValue(retVal, 1, "va_arg_str_len"), 1, "str_len");
            return strStruct;
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
            auto* i8PtrTy = llvm::PointerType::getUnqual(context);
            auto* vaEndFunc = llvm::Intrinsic::getOrInsertDeclaration(module, llvm::Intrinsic::vaend, {i8PtrTy});
            auto* vaArg = builder.CreateBitCast(args[0], i8PtrTy);
            
            builder.CreateCall(vaEndFunc, {vaArg});
            return llvm::UndefValue::get(llvm::Type::getInt32Ty(context));
        };

        unsafe.functions.push_back(std::move(fn));
    }

    reg.registerNamespace(std::move(unsafe));
}
