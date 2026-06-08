#pragma once

#include <memory>
#include <string>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

// Owns both the LLVMContext and the Module.
// The context must outlive the module, so they are held together here.
class IRModule {
public:
    IRModule(std::unique_ptr<llvm::LLVMContext> context,
             std::unique_ptr<llvm::Module>      module);

    llvm::LLVMContext& context() { return *context_; }
    llvm::Module*      module()  { return module_.get(); }

    std::unique_ptr<llvm::LLVMContext> takeContext() { return std::move(context_); }
    std::unique_ptr<llvm::Module>      takeModule()  { return std::move(module_); }

    // Print LLVM IR to stdout.
    void        print()    const;
    // Return LLVM IR as a string.
    std::string toString() const;

private:
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module>      module_;
};
