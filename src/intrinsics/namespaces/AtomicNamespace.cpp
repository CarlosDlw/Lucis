#include "intrinsics/IntrinsicRegistry.h"
#include "types/TypeInfo.h"
#include "types/TypeRegistry.h"

#include <functional>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/IRBuilder.h>

void registerAtomicNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg) {

    IntrinsicNamespace atomic;
    atomic.name = "atomic";
    atomic.description =
        "Atomic memory operations with configurable ordering.\n"
        "Provides atomic load, store, fetch-and-{add,sub,and,or,xor}, "
        "exchange, and compare-and-swap.\n\n"
        "Default variants use sequential consistency (seq_cst).\n"
        "Suffixed variants: _acq (acquire), _rel (release), "
        "_acqrel (acq_rel), _rlx (relaxed).\n"
        "Always available without any `use` declaration.";

    auto makeGenIntrinsic = [](
        const std::string& name,
        const std::string& returnType,
        std::vector<IntrinsicParam> params,
        const std::string& desc,
        std::function<llvm::Value*(
            llvm::IRBuilder<>&,
            llvm::LLVMContext&,
            llvm::Module*,
            const TypeInfo*,
            const std::vector<llvm::Value*>&)> body)
    {
        IntrinsicFunction fn;
        fn.name = name;
        fn.returnType = returnType;
        fn.isGeneric = true;
        fn.params = std::move(params);
        fn.description = desc;

        fn.lowering.kind = IntrinsicFunction::Lowering::InlineIR;
        fn.lowering.emitIR = [body = std::move(body)](
            llvm::IRBuilder<>& builder,
            llvm::Module* module,
            llvm::LLVMContext& context,
            const TypeRegistry& typeRegistry,
            const std::vector<llvm::Value*>& args,
            const std::vector<const TypeInfo*>& typeArgs) -> llvm::Value* {

            if (typeArgs.empty()) {
                llvm::report_fatal_error("atomic intrinsic requires a type argument");
            }
            auto* elemTy = typeArgs[0]->toLLVMType(context, module->getDataLayout());
            if (!elemTy)
                llvm::report_fatal_error("atomic intrinsic: cannot resolve element type");

            // Wrap the body to pass the resolved element type
            // The body already captures the type via the lambda
            return body(builder, context, module, typeArgs[0], args);
        };

        return fn;
    };

    // ── atomic_load<T>(ptr) -> T ─────────────────────────────────
    {
        auto fn = makeGenIntrinsic("load", "_any",
            {{"_any", false}},
            "Atomically loads a value of type T from ptr (seq_cst).\n\n"
            "```lucis\n"
            "int32 val = lucis::atomic::load<int32>(&data);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                auto* elemTy = elemTI->toLLVMType(context, module->getDataLayout());
                auto* load = builder.CreateAlignedLoad(elemTy, args[0],
                    llvm::MaybeAlign());
                load->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent);
                return load;
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_store<T>(ptr, val) ────────────────────────────────
    {
        auto fn = makeGenIntrinsic("store", "void",
            {{"_any", false}, {"_any", false}},
            "Atomically stores a value of type T to ptr (seq_cst).\n\n"
            "```lucis\n"
            "lucis::atomic::store<int32>(&data, 42);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                auto* elemTy = elemTI->toLLVMType(context, module->getDataLayout());
                auto* store = builder.CreateAlignedStore(args[1], args[0],
                    llvm::MaybeAlign());
                store->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent);
                return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_add<T>(ptr, val) -> T (fetch_add, returns old) ────
    {
        auto fn = makeGenIntrinsic("add", "_any",
            {{"_any", false}, {"_any", false}},
            "Atomically adds val to *ptr (seq_cst, fetch_add semantics).\n"
            "Returns the old value.\n\n"
            "```lucis\n"
            "int32 old = lucis::atomic::add<int32>(&counter, 1);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                return builder.CreateAtomicRMW(
                    llvm::AtomicRMWInst::Add, args[0], args[1],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::SequentiallyConsistent);
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_sub<T>(ptr, val) -> T (fetch_sub, returns old) ────
    {
        auto fn = makeGenIntrinsic("sub", "_any",
            {{"_any", false}, {"_any", false}},
            "Atomically subtracts val from *ptr (seq_cst, fetch_sub semantics).\n"
            "Returns the old value.\n\n"
            "```lucis\n"
            "int32 old = lucis::atomic::sub<int32>(&counter, 1);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                return builder.CreateAtomicRMW(
                    llvm::AtomicRMWInst::Sub, args[0], args[1],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::SequentiallyConsistent);
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_bit_and<T>(ptr, val) -> T (fetch_and, returns old) ─
    {
        auto fn = makeGenIntrinsic("bit_and", "_any",
            {{"_any", false}, {"_any", false}},
            "Atomically ANDs val with *ptr (seq_cst, fetch_and semantics).\n"
            "Returns the old value.\n\n"
            "```lucis\n"
            "int32 old = lucis::atomic::bit_and<int32>(&flags, 0xFE);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                return builder.CreateAtomicRMW(
                    llvm::AtomicRMWInst::And, args[0], args[1],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::SequentiallyConsistent);
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_bit_or<T>(ptr, val) -> T (fetch_or, returns old) ──
    {
        auto fn = makeGenIntrinsic("bit_or", "_any",
            {{"_any", false}, {"_any", false}},
            "Atomically ORs val with *ptr (seq_cst, fetch_or semantics).\n"
            "Returns the old value.\n\n"
            "```lucis\n"
            "int32 old = lucis::atomic::bit_or<int32>(&flags, 0x01);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                return builder.CreateAtomicRMW(
                    llvm::AtomicRMWInst::Or, args[0], args[1],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::SequentiallyConsistent);
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_bit_xor<T>(ptr, val) -> T (fetch_xor, returns old) ─
    {
        auto fn = makeGenIntrinsic("bit_xor", "_any",
            {{"_any", false}, {"_any", false}},
            "Atomically XORs val with *ptr (seq_cst, fetch_xor semantics).\n"
            "Returns the old value.\n\n"
            "```lucis\n"
            "int32 old = lucis::atomic::bit_xor<int32>(&flags, 0xFF);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                return builder.CreateAtomicRMW(
                    llvm::AtomicRMWInst::Xor, args[0], args[1],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::SequentiallyConsistent);
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_exchange<T>(ptr, val) -> T (swap, returns old) ────
    {
        auto fn = makeGenIntrinsic("exchange", "_any",
            {{"_any", false}, {"_any", false}},
            "Atomically replaces *ptr with val (seq_cst, swap semantics).\n"
            "Returns the old value.\n\n"
            "```lucis\n"
            "int32 old = lucis::atomic::exchange<int32>(&data, 42);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                return builder.CreateAtomicRMW(
                    llvm::AtomicRMWInst::Xchg, args[0], args[1],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::SequentiallyConsistent);
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_cas<T>(ptr, expected, desired) -> bool ────────────
    {
        auto fn = makeGenIntrinsic("cas", "bool",
            {{"_any", false}, {"_any", false}, {"_any", false}},
            "Atomically compares *ptr with expected and, if equal, "
            "replaces with desired (seq_cst, weak CAS).\n"
            "Returns true if the swap occurred.\n\n"
            "```lucis\n"
            "bool swapped = lucis::atomic::cas<int32>(&data, 0i64, 42i64);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                auto* cas = builder.CreateAtomicCmpXchg(
                    args[0], args[1], args[2],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::SequentiallyConsistent,
                    llvm::AtomicOrdering::SequentiallyConsistent);
                return builder.CreateExtractValue(cas, {1}, "cas_ok");
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── Ordering variants ─────────────────────────────────────────

    // ── atomic_load_acq<T>(ptr) -> T  (acquire) ──────────────────
    {
        auto fn = makeGenIntrinsic("load_acq", "_any",
            {{"_any", false}},
            "Atomically loads a value of type T from ptr (acquire ordering).\n"
            "Prevents subsequent loads/stores from being reordered before this load.\n\n"
            "```lucis\n"
            "int32 val = lucis::atomic::load_acq<int32>(&data);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                auto* elemTy = elemTI->toLLVMType(context, module->getDataLayout());
                auto* load = builder.CreateAlignedLoad(elemTy, args[0],
                    llvm::MaybeAlign());
                load->setAtomic(llvm::AtomicOrdering::Acquire);
                return load;
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_load_rlx<T>(ptr) -> T  (relaxed) ──────────────────
    {
        auto fn = makeGenIntrinsic("load_rlx", "_any",
            {{"_any", false}},
            "Atomically loads a value of type T from ptr (relaxed ordering).\n"
            "No ordering guarantees; only atomicity is ensured.\n\n"
            "```lucis\n"
            "int32 val = lucis::atomic::load_rlx<int32>(&data);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                auto* elemTy = elemTI->toLLVMType(context, module->getDataLayout());
                auto* load = builder.CreateAlignedLoad(elemTy, args[0],
                    llvm::MaybeAlign());
                load->setAtomic(llvm::AtomicOrdering::Monotonic);
                return load;
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_store_rel<T>(ptr, val)  (release) ─────────────────
    {
        auto fn = makeGenIntrinsic("store_rel", "void",
            {{"_any", false}, {"_any", false}},
            "Atomically stores a value of type T to ptr (release ordering).\n"
            "Prevents preceding loads/stores from being reordered after this store.\n\n"
            "```lucis\n"
            "lucis::atomic::store_rel<int32>(&data, 42);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                auto* elemTy = elemTI->toLLVMType(context, module->getDataLayout());
                auto* store = builder.CreateAlignedStore(args[1], args[0],
                    llvm::MaybeAlign());
                store->setAtomic(llvm::AtomicOrdering::Release);
                return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_store_rlx<T>(ptr, val)  (relaxed) ─────────────────
    {
        auto fn = makeGenIntrinsic("store_rlx", "void",
            {{"_any", false}, {"_any", false}},
            "Atomically stores a value of type T to ptr (relaxed ordering).\n"
            "No ordering guarantees; only atomicity is ensured.\n\n"
            "```lucis\n"
            "lucis::atomic::store_rlx<int32>(&data, 42);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                auto* elemTy = elemTI->toLLVMType(context, module->getDataLayout());
                auto* store = builder.CreateAlignedStore(args[1], args[0],
                    llvm::MaybeAlign());
                store->setAtomic(llvm::AtomicOrdering::Monotonic);
                return llvm::UndefValue::get(llvm::Type::getVoidTy(context));
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_add_acqrel<T>(ptr, val) -> T  (acq_rel, fetch_add) ─
    {
        auto fn = makeGenIntrinsic("add_acqrel", "_any",
            {{"_any", false}, {"_any", false}},
            "Atomically adds val to *ptr (acq_rel ordering, fetch_add semantics).\n"
            "Returns the old value. Suitable for reference counting.\n\n"
            "```lucis\n"
            "int32 old = lucis::atomic::add_acqrel<int32>(&counter, 1);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                return builder.CreateAtomicRMW(
                    llvm::AtomicRMWInst::Add, args[0], args[1],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::AcquireRelease);
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_add_rlx<T>(ptr, val) -> T  (relaxed, fetch_add) ──
    {
        auto fn = makeGenIntrinsic("add_rlx", "_any",
            {{"_any", false}, {"_any", false}},
            "Atomically adds val to *ptr (relaxed ordering, fetch_add semantics).\n"
            "Returns the old value. Suitable for simple counters.\n\n"
            "```lucis\n"
            "int32 old = lucis::atomic::add_rlx<int32>(&counter, 1);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                return builder.CreateAtomicRMW(
                    llvm::AtomicRMWInst::Add, args[0], args[1],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::Monotonic);
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_sub_acqrel<T>(ptr, val) -> T  (acq_rel, fetch_sub) ─
    {
        auto fn = makeGenIntrinsic("sub_acqrel", "_any",
            {{"_any", false}, {"_any", false}},
            "Atomically subtracts val from *ptr (acq_rel ordering, fetch_sub).\n"
            "Returns the old value.\n\n"
            "```lucis\n"
            "int32 old = lucis::atomic::sub_acqrel<int32>(&counter, 1);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                return builder.CreateAtomicRMW(
                    llvm::AtomicRMWInst::Sub, args[0], args[1],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::AcquireRelease);
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_sub_rlx<T>(ptr, val) -> T  (relaxed, fetch_sub) ──
    {
        auto fn = makeGenIntrinsic("sub_rlx", "_any",
            {{"_any", false}, {"_any", false}},
            "Atomically subtracts val from *ptr (relaxed ordering, fetch_sub).\n"
            "Returns the old value.\n\n"
            "```lucis\n"
            "int32 old = lucis::atomic::sub_rlx<int32>(&counter, 1);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                return builder.CreateAtomicRMW(
                    llvm::AtomicRMWInst::Sub, args[0], args[1],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::Monotonic);
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_xchg_acqrel<T>(ptr, val) -> T  (acq_rel, swap) ───
    {
        auto fn = makeGenIntrinsic("xchg_acqrel", "_any",
            {{"_any", false}, {"_any", false}},
            "Atomically swaps *ptr with val (acq_rel ordering).\n"
            "Returns the old value. Suitable for spinlocks.\n\n"
            "```lucis\n"
            "int32 old = lucis::atomic::xchg_acqrel<int32>(&lock, 1);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                return builder.CreateAtomicRMW(
                    llvm::AtomicRMWInst::Xchg, args[0], args[1],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::AcquireRelease);
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_xchg_rlx<T>(ptr, val) -> T  (relaxed, swap) ──────
    {
        auto fn = makeGenIntrinsic("xchg_rlx", "_any",
            {{"_any", false}, {"_any", false}},
            "Atomically swaps *ptr with val (relaxed ordering).\n"
            "Returns the old value.\n\n"
            "```lucis\n"
            "int32 old = lucis::atomic::xchg_rlx<int32>(&data, 42);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                return builder.CreateAtomicRMW(
                    llvm::AtomicRMWInst::Xchg, args[0], args[1],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::Monotonic);
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_cas_acqrel<T>(ptr, expected, desired) -> bool ─────
    {
        auto fn = makeGenIntrinsic("cas_acqrel", "bool",
            {{"_any", false}, {"_any", false}, {"_any", false}},
            "Atomically compares *ptr with expected and swaps if equal "
            "(acq_rel success, acquire failure, weak CAS).\n"
            "Returns true if the swap occurred.\n\n"
            "```lucis\n"
            "bool ok = lucis::atomic::cas_acqrel<int32>(&data, old, new);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                auto* cas = builder.CreateAtomicCmpXchg(
                    args[0], args[1], args[2],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::AcquireRelease,
                    llvm::AtomicOrdering::Acquire);
                return builder.CreateExtractValue(cas, {1}, "cas_ok");
            });
        atomic.functions.push_back(std::move(fn));
    }

    // ── atomic_cas_rlx<T>(ptr, expected, desired) -> bool ────────
    {
        auto fn = makeGenIntrinsic("cas_rlx", "bool",
            {{"_any", false}, {"_any", false}, {"_any", false}},
            "Atomically compares *ptr with expected and swaps if equal "
            "(relaxed ordering, weak CAS).\n"
            "Returns true if the swap occurred.\n\n"
            "```lucis\n"
            "bool ok = lucis::atomic::cas_rlx<int32>(&data, old, new);\n"
            "```",
            [](llvm::IRBuilder<>& builder, llvm::LLVMContext& context,
               llvm::Module* module, const TypeInfo* elemTI,
               const std::vector<llvm::Value*>& args) -> llvm::Value* {
                auto* cas = builder.CreateAtomicCmpXchg(
                    args[0], args[1], args[2],
                    llvm::MaybeAlign(),
                    llvm::AtomicOrdering::Monotonic,
                    llvm::AtomicOrdering::Monotonic);
                return builder.CreateExtractValue(cas, {1}, "cas_ok");
            });
        atomic.functions.push_back(std::move(fn));
    }

    reg.registerNamespace(std::move(atomic));
}
