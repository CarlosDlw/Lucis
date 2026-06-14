#include "intrinsics/IntrinsicRegistry.h"
#include "types/TypeInfo.h"
#include "types/TypeRegistry.h"

#include <functional>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/IRBuilder.h>

void registerIoNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg) {

    IntrinsicNamespace io;
    io.name = "io";
    io.description =
        "Low-level file I/O intrinsics.\n"
        "Provides direct access to file descriptors: open, read, write, close, lseek.\n\n"
        "Cross-platform — backed by C runtime wrappers with #ifdef _WIN32.\n"
        "Always available without any `use` declaration.";

    // Helper: declare or get an external C function in the LLVM module
    auto declareCFunc = [](
        llvm::Module* module,
        llvm::LLVMContext& context,
        const std::string& name,
        llvm::Type* retTy,
        std::vector<llvm::Type*> paramTys) -> llvm::Function* {

        // Check if already declared
        auto* existing = module->getFunction(name);
        if (existing) return existing;

        auto* ft = llvm::FunctionType::get(retTy, paramTys, false);
        auto* fn = llvm::Function::Create(
            ft, llvm::Function::ExternalLinkage, name, module);
        fn->setDSOLocal(true);
        return fn;
    };

    // ── open(path, flags, mode) → int32 ───────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "open";
        fn.returnType = "int32";
        fn.params.push_back({"_any", false});
        fn.params.push_back({"int32", false});
        fn.params.push_back({"int32", false});
        fn.description =
            "Opens a file and returns a file descriptor.\n"
            "Flags and mode follow POSIX conventions (O_RDONLY=0, O_WRONLY=1, "
            "O_RDWR=2, O_CREAT=0x40, O_TRUNC=0x200, O_APPEND=0x400).\n"
            "Returns -1 on error.\n\n"
            "```lucis\n"
            "int32 fd = lucis::io::open(c\"file.txt\", 0, 0);  // O_RDONLY\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [declareCFunc](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* i32Ty = llvm::Type::getInt32Ty(context);
            auto* ptrTy = llvm::PointerType::getUnqual(context);
            auto* callee = declareCFunc(module, context, "lucis_open",
                i32Ty, {ptrTy, i32Ty, i32Ty});
            return builder.CreateCall(callee, args, "fd");
        };

        io.functions.push_back(std::move(fn));
    }

    // ── read(fd, buf, count) → int32 ─────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "read";
        fn.returnType = "int32";
        fn.params.push_back({"int32", false});
        fn.params.push_back({"_any", false});
        fn.params.push_back({"int32", false});
        fn.description =
            "Reads up to count bytes from a file descriptor into buf.\n"
            "Returns the number of bytes read, 0 at EOF, -1 on error.\n\n"
            "```lucis\n"
            "int32 n = lucis::io::read(fd, &buf, 256);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [declareCFunc](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* i32Ty = llvm::Type::getInt32Ty(context);
            auto* ptrTy = llvm::PointerType::getUnqual(context);
            auto* callee = declareCFunc(module, context, "lucis_read",
                i32Ty, {i32Ty, ptrTy, i32Ty});
            return builder.CreateCall(callee, args, "nread");
        };

        io.functions.push_back(std::move(fn));
    }

    // ── write(fd, buf, count) → int32 ────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "write";
        fn.returnType = "int32";
        fn.params.push_back({"int32", false});
        fn.params.push_back({"_any", false});
        fn.params.push_back({"int32", false});
        fn.description =
            "Writes up to count bytes from buf to a file descriptor.\n"
            "Returns the number of bytes written, -1 on error.\n\n"
            "```lucis\n"
            "int32 n = lucis::io::write(fd, &buf, 256);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [declareCFunc](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* i32Ty = llvm::Type::getInt32Ty(context);
            auto* ptrTy = llvm::PointerType::getUnqual(context);
            auto* callee = declareCFunc(module, context, "lucis_write",
                i32Ty, {i32Ty, ptrTy, i32Ty});
            return builder.CreateCall(callee, args, "nwritten");
        };

        io.functions.push_back(std::move(fn));
    }

    // ── close(fd) → int32 ────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "close";
        fn.returnType = "int32";
        fn.params.push_back({"int32", false});
        fn.description =
            "Closes a file descriptor.\n"
            "Returns 0 on success, -1 on error.\n\n"
            "```lucis\n"
            "lucis::io::close(fd);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [declareCFunc](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* i32Ty = llvm::Type::getInt32Ty(context);
            auto* callee = declareCFunc(module, context, "lucis_close",
                i32Ty, {i32Ty});
            return builder.CreateCall(callee, args, "ret");
        };

        io.functions.push_back(std::move(fn));
    }

    // ── lseek(fd, offset, whence) → int64 ────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "lseek";
        fn.returnType = "int64";
        fn.params.push_back({"int32", false});
        fn.params.push_back({"int64", false});
        fn.params.push_back({"int32", false});
        fn.description =
            "Repositions the file offset of an open file descriptor.\n"
            "Whence: SEEK_SET=0, SEEK_CUR=1, SEEK_END=2.\n"
            "Returns the resulting offset, -1 on error.\n\n"
            "```lucis\n"
            "int64 pos = lucis::io::lseek(fd, 0, 0);  // SEEK_SET\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [declareCFunc](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* i32Ty = llvm::Type::getInt32Ty(context);
            auto* i64Ty = llvm::Type::getInt64Ty(context);
            auto* callee = declareCFunc(module, context, "lucis_lseek",
                i64Ty, {i32Ty, i64Ty, i32Ty});
            return builder.CreateCall(callee, args, "pos");
        };

        io.functions.push_back(std::move(fn));
    }

    // ── fsync(fd) → int32 ────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "fsync";
        fn.returnType = "int32";
        fn.params.push_back({"int32", false});
        fn.description =
            "Synchronises a file's in-core state with storage device.\n"
            "Returns 0 on success, -1 on error.\n\n"
            "```lucis\n"
            "lucis::io::fsync(fd);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [declareCFunc](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* i32Ty = llvm::Type::getInt32Ty(context);
            auto* callee = declareCFunc(module, context, "lucis_fsync",
                i32Ty, {i32Ty});
            return builder.CreateCall(callee, args, "ret");
        };

        io.functions.push_back(std::move(fn));
    }

    // ── unlink(path) → int32 ──────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "unlink";
        fn.returnType = "int32";
        fn.params.push_back({"_any", false});
        fn.description =
            "Deletes a file from the filesystem.\n"
            "Returns 0 on success, -1 on error.\n\n"
            "```lucis\n"
            "lucis::io::unlink(c\"file.txt\");\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [declareCFunc](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* i32Ty = llvm::Type::getInt32Ty(context);
            auto* ptrTy = llvm::PointerType::getUnqual(context);
            auto* callee = declareCFunc(module, context, "lucis_unlink",
                i32Ty, {ptrTy});
            return builder.CreateCall(callee, args, "ret");
        };

        io.functions.push_back(std::move(fn));
    }

    // ── file_size(fd) → int64 ─────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "file_size";
        fn.returnType = "int64";
        fn.params.push_back({"int32", false});
        fn.description =
            "Returns the size of an open file in bytes.\n"
            "Equivalent to fstat + st_size, returns -1 on error.\n\n"
            "```lucis\n"
            "int64 sz = lucis::io::file_size(fd);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [declareCFunc](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* i32Ty = llvm::Type::getInt32Ty(context);
            auto* i64Ty = llvm::Type::getInt64Ty(context);
            auto* callee = declareCFunc(module, context, "lucis_fileSize_fd",
                i64Ty, {i32Ty});
            return builder.CreateCall(callee, args, "sz");
        };

        io.functions.push_back(std::move(fn));
    }

    // ── dup(fd) → int32 ──────────────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "dup";
        fn.returnType = "int32";
        fn.params.push_back({"int32", false});
        fn.description =
            "Duplicates a file descriptor.\n"
            "Returns a new fd (the lowest-numbered unused fd), -1 on error.\n\n"
            "```lucis\n"
            "int32 newfd = lucis::io::dup(fd);\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [declareCFunc](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* i32Ty = llvm::Type::getInt32Ty(context);
            auto* callee = declareCFunc(module, context, "lucis_dup",
                i32Ty, {i32Ty});
            return builder.CreateCall(callee, args, "newfd");
        };

        io.functions.push_back(std::move(fn));
    }

    // ── dup2(oldfd, newfd) → int32 ────────────────────────────────
    {
        IntrinsicFunction fn;
        fn.name = "dup2";
        fn.returnType = "int32";
        fn.params.push_back({"int32", false});
        fn.params.push_back({"int32", false});
        fn.description =
            "Duplicates a file descriptor to a specific fd number.\n"
            "If newfd is already open, it is closed first.\n"
            "Returns newfd on success, -1 on error.\n\n"
            "```lucis\n"
            "lucis::io::dup2(fd, 1);  // redirect fd → stdout\n"
            "```";

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [declareCFunc](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            auto* i32Ty = llvm::Type::getInt32Ty(context);
            auto* callee = declareCFunc(module, context, "lucis_dup2",
                i32Ty, {i32Ty, i32Ty});
            return builder.CreateCall(callee, args, "ret");
        };

        io.functions.push_back(std::move(fn));
    }

    reg.registerNamespace(std::move(io));
}
