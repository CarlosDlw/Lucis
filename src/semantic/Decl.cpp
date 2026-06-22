#include "semantic/Decl.h"

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>

#include <algorithm>

namespace semantic {

// ── Helpers ───────────────────────────────────────────────────────────────────

static constexpr unsigned kOpaqueUnsizedArrayPayloadBytes = 4096;

static void cloneFields(std::vector<FieldInfo>& dst,
                        const std::vector<FieldInfo>& src) {
    dst.reserve(src.size());
    for (const auto& f : src) {
        FieldInfo copy;
        copy.name       = f.name;
        copy.type       = f.type;
        copy.arrayDims  = f.arrayDims;
        copy.arraySizes = f.arraySizes;
        copy.autoFill   = f.autoFill;
        dst.push_back(std::move(copy));
    }
}

static void cloneMethods(std::vector<MethodInfo>& dst,
                         const std::vector<MethodInfo>& src) {
    dst.reserve(src.size());
    for (const auto& m : src) {
        MethodInfo copy;
        copy.name       = m.name;
        copy.returnType = m.returnType;
        copy.isStatic   = m.isStatic;
        for (const auto& p : m.params) {
            copy.params.push_back({p.name, p.type});
        }
        dst.push_back(std::move(copy));
    }
}

static void cloneVariants(std::vector<VariantInfo>& dst,
                          const std::vector<VariantInfo>& src) {
    dst.reserve(src.size());
    for (const auto& v : src) {
        VariantInfo copy;
        copy.name          = v.name;
        copy.discriminant  = v.discriminant;
        copy.payloadKind   = v.payloadKind;
        cloneFields(copy.payloadFields, v.payloadFields);
        dst.push_back(std::move(copy));
    }
}

template <typename F>
static void substituteInFields(std::vector<FieldInfo>& fields,
                               const std::unordered_map<std::string, Decl*>& subst,
                               F&& resolveType) {
    for (auto& f : fields) {
        if (!f.type) continue;
        auto it = subst.find(f.type->name);
        if (it != subst.end())
            f.type = it->second;
    }
}

template <typename F>
static void substituteInMethods(std::vector<MethodInfo>& methods,
                                const std::unordered_map<std::string, Decl*>& subst,
                                F&& resolveType) {
    for (auto& m : methods) {
        if (m.returnType) {
            auto it = subst.find(m.returnType->name);
            if (it != subst.end())
                m.returnType = it->second;
        }
        for (auto& p : m.params) {
            if (!p.type) continue;
            auto it = subst.find(p.type->name);
            if (it != subst.end())
                p.type = it->second;
        }
    }
}

static std::string makeCloneName(const std::string& base) {
    return base; // clones keep their original name; mangling done by SemanticDB
}

// ── Decl base ─────────────────────────────────────────────────────────────────

bool Decl::isPrimitive() const {
    switch (kind) {
    case DeclKind::Integer: case DeclKind::Float:
    case DeclKind::Bool:    case DeclKind::Char:
    case DeclKind::Void:    case DeclKind::String:
    case DeclKind::VAList:
        return true;
    default:
        return false;
    }
}

llvm::Type* Decl::toLLVMType(llvm::LLVMContext& ctx,
                             const llvm::DataLayout& dl) const {
    // Forward to the subtype.  Each concrete Decl type MUST override this
    // because the LLVM type depends on the concrete fields.
    //
    // Primitive types are handled directly in the switch below.
    //
    // Struct / Union delegates to llvm::StructType::getTypeByName() — the
    // concrete body is set later by IRGen.
    //
    // We deliberately do NOT use a virtual dispatch here; the large switch
    // makes it easy to verify that every DeclKind is handled.

    switch (kind) {
    case DeclKind::Bool:
        return llvm::Type::getInt1Ty(ctx);
    case DeclKind::Char:
        return llvm::Type::getInt8Ty(ctx);
    case DeclKind::Integer: {
        auto* pi = static_cast<const PrimitiveDecl*>(this);
        if (pi->bitWidth == 0)
            return dl.getIntPtrType(ctx);
        return llvm::Type::getIntNTy(ctx, pi->bitWidth);
    }
    case DeclKind::Float: {
        auto* pi = static_cast<const PrimitiveDecl*>(this);
        switch (pi->bitWidth) {
        case 128: return llvm::Type::getFP128Ty(ctx);
        case 80:  return llvm::Type::getX86_FP80Ty(ctx);
        case 64:  return llvm::Type::getDoubleTy(ctx);
        default:  return llvm::Type::getFloatTy(ctx);
        }
    }
    case DeclKind::Void:
        return llvm::Type::getVoidTy(ctx);
    case DeclKind::String:
        return llvm::StructType::get(ctx, {
            llvm::PointerType::getUnqual(ctx),
            dl.getIntPtrType(ctx)
        });
    case DeclKind::Struct:
    case DeclKind::Union:
        return llvm::StructType::getTypeByName(ctx, name);
    case DeclKind::Pointer:
        return llvm::PointerType::getUnqual(ctx);
    case DeclKind::VAList: {
        auto* existing = llvm::StructType::getTypeByName(ctx, "struct.__va_list_tag");
        if (existing) return existing;
        return llvm::StructType::create(ctx, {
            llvm::Type::getInt32Ty(ctx),
            llvm::Type::getInt32Ty(ctx),
            llvm::PointerType::getUnqual(ctx),
            llvm::PointerType::getUnqual(ctx)
        }, "struct.__va_list_tag");
    }
    case DeclKind::Function:
        return llvm::PointerType::getUnqual(ctx);
    case DeclKind::Tuple: {
        auto* td = static_cast<const TupleDecl*>(this);
        std::vector<llvm::Type*> elemTypes;
        for (auto* elem : td->elements)
            elemTypes.push_back(elem->toLLVMType(ctx, dl));
        return llvm::StructType::get(ctx, elemTypes);
    }
    case DeclKind::Enum: {
        auto* ed = static_cast<const EnumDecl*>(this);
        if (!ed->hasPayload())
            return llvm::Type::getInt32Ty(ctx);

        auto* existing = llvm::StructType::getTypeByName(ctx, ed->name);
        if (existing) return existing;

        llvm::Type*    storageTy = nullptr;
        llvm::TypeSize maxSize   = llvm::TypeSize::getFixed(0);
        llvm::Align    maxAlign(1);

        for (const auto& variant : ed->variants) {
            if (variant.payloadFields.empty()) continue;

            std::vector<llvm::Type*> fieldTypes;
            for (const auto& f : variant.payloadFields) {
                auto* baseTy = f.type ? f.type->toLLVMType(ctx, dl)
                                      : llvm::Type::getInt32Ty(ctx);
                llvm::Type* ft = baseTy;
                if (f.arrayDims > 0) {
                    if (f.arraySizes.empty()) {
                        ft = llvm::ArrayType::get(
                            llvm::Type::getInt8Ty(ctx),
                            kOpaqueUnsizedArrayPayloadBytes);
                    } else {
                        for (auto it = f.arraySizes.rbegin();
                             it != f.arraySizes.rend(); ++it)
                            ft = llvm::ArrayType::get(ft, *it);
                    }
                }
                fieldTypes.push_back(ft);
            }

            llvm::Type* payloadTy = nullptr;
            if (variant.payloadKind == VariantPayloadKind::Tuple &&
                fieldTypes.size() == 1)
                payloadTy = fieldTypes[0];
            else
                payloadTy = llvm::StructType::get(ctx, fieldTypes);

            auto size  = dl.getTypeAllocSize(payloadTy);
            auto align = dl.getABITypeAlign(payloadTy);
            if (!storageTy || size > maxSize ||
                (size == maxSize && align > maxAlign)) {
                storageTy = payloadTy;
                maxSize   = size;
                maxAlign  = align;
            }
        }

        if (!storageTy)
            return llvm::Type::getInt32Ty(ctx);

        return llvm::StructType::create(ctx, {
            llvm::Type::getInt32Ty(ctx), storageTy
        }, ed->name);
    }
    case DeclKind::Extended: {
        auto* ex = static_cast<const ExtendedDecl*>(this);
        if (ex->extendedKind == "Task" || ex->extendedKind == "Mutex")
            return llvm::PointerType::getUnqual(ctx);
        if (ex->extendedKind == "Map") {
            auto* existing = llvm::StructType::getTypeByName(ctx, "lucis_map_header");
            if (existing) return existing;
            auto* ptrTy   = llvm::PointerType::getUnqual(ctx);
            auto* usizeTy = dl.getIntPtrType(ctx);
            return llvm::StructType::create(ctx, {
                ptrTy, ptrTy, ptrTy, ptrTy,
                usizeTy, usizeTy, usizeTy, usizeTy
            }, "lucis_map_header");
        }
        if (ex->extendedKind == "Set") {
            auto* existing = llvm::StructType::getTypeByName(ctx, "lucis_set_header");
            if (existing) return existing;
            auto* ptrTy   = llvm::PointerType::getUnqual(ctx);
            auto* usizeTy = dl.getIntPtrType(ctx);
            return llvm::StructType::create(ctx, {
                ptrTy, ptrTy, ptrTy,
                usizeTy, usizeTy, usizeTy
            }, "lucis_set_header");
        }
        // Vec<T> and all other extended types
        auto* existing = llvm::StructType::getTypeByName(ctx, "lucis_vec_header");
        if (existing) return existing;
        return llvm::StructType::create(ctx, {
            llvm::PointerType::getUnqual(ctx),
            dl.getIntPtrType(ctx),
            dl.getIntPtrType(ctx)
        }, "lucis_vec_header");
    }
    case DeclKind::TypeAlias: {
        auto* ta = static_cast<const TypeAliasDecl*>(this);
        // Fixed-size inline array alias: [N]T
        if (ta->arraySize > 0 && ta->target) {
            auto* elemTy = ta->target->toLLVMType(ctx, dl);
            return llvm::ArrayType::get(elemTy, ta->arraySize);
        }
        // Multi-dim array alias: [N][M]T
        if (!ta->arraySizes.empty() && ta->target) {
            auto* elemTy = ta->target->toLLVMType(ctx, dl);
            llvm::Type* arrTy = elemTy;
            for (auto it = ta->arraySizes.rbegin();
                 it != ta->arraySizes.rend(); ++it)
                arrTy = llvm::ArrayType::get(arrTy, *it);
            return arrTy;
        }
        return ta->target ? ta->target->toLLVMType(ctx, dl)
                          : llvm::Type::getInt32Ty(ctx);
    }
    case DeclKind::GenericTemplate:
        // Templates don't have an LLVM type; instantiate first.
        return llvm::Type::getVoidTy(ctx);
    case DeclKind::Intrinsic:
        return llvm::PointerType::getUnqual(ctx);
    }

    return llvm::Type::getInt32Ty(ctx); // unreachable fallback
}

// ── PrimitiveDecl ─────────────────────────────────────────────────────────────

std::unique_ptr<Decl> PrimitiveDecl::clone() const {
    auto c = std::make_unique<PrimitiveDecl>(kind);
    c->name          = name;
    c->modulePath    = modulePath;
    c->loc           = loc;
    c->dropTracked   = dropTracked;
    c->moveOnly      = moveOnly;
    c->bitWidth      = bitWidth;
    c->isSigned      = isSigned;
    c->builtinSuffix = builtinSuffix;
    return c;
}

// ── StructDecl ────────────────────────────────────────────────────────────────

std::unique_ptr<Decl> StructDecl::clone() const {
    auto c = std::make_unique<StructDecl>();
    c->name          = name;
    c->modulePath    = modulePath;
    c->loc           = loc;
    c->dropTracked   = dropTracked;
    c->moveOnly      = moveOnly;
    c->genericParams = genericParams;
    c->parentName    = parentName;
    cloneFields(c->fields, fields);
    cloneMethods(c->methods, methods);
    return c;
}

void StructDecl::substituteTypes(
    const std::unordered_map<std::string, Decl*>& subst) {
    substituteInFields(fields, subst, [](Decl*) {});
    substituteInMethods(methods, subst, [](Decl*) {});
}

// ── UnionDecl ─────────────────────────────────────────────────────────────────

std::unique_ptr<Decl> UnionDecl::clone() const {
    auto c = std::make_unique<UnionDecl>();
    c->name          = name;
    c->modulePath    = modulePath;
    c->loc           = loc;
    c->dropTracked   = dropTracked;
    c->moveOnly      = moveOnly;
    c->genericParams = genericParams;
    cloneFields(c->fields, fields);
    return c;
}

void UnionDecl::substituteTypes(
    const std::unordered_map<std::string, Decl*>& subst) {
    substituteInFields(fields, subst, [](Decl*) {});
}

// ── EnumDecl ──────────────────────────────────────────────────────────────────

bool EnumDecl::hasPayload() const {
    return std::any_of(variants.begin(), variants.end(),
        [](const VariantInfo& v) { return !v.payloadFields.empty(); });
}

std::unique_ptr<Decl> EnumDecl::clone() const {
    auto c = std::make_unique<EnumDecl>();
    c->name          = name;
    c->modulePath    = modulePath;
    c->loc           = loc;
    c->dropTracked   = dropTracked;
    c->moveOnly      = moveOnly;
    c->genericParams = genericParams;
    c->bitWidth      = bitWidth;
    cloneVariants(c->variants, variants);
    return c;
}

void EnumDecl::substituteTypes(
    const std::unordered_map<std::string, Decl*>& subst) {
    for (auto& v : variants)
        substituteInFields(v.payloadFields, subst, [](Decl*) {});
}

// ── PointerDecl ───────────────────────────────────────────────────────────────

std::unique_ptr<Decl> PointerDecl::clone() const {
    auto c = std::make_unique<PointerDecl>();
    c->name          = name;
    c->modulePath    = modulePath;
    c->loc           = loc;
    c->dropTracked   = dropTracked;
    c->moveOnly      = moveOnly;
    c->pointeeType   = pointeeType;
    return c;
}

void PointerDecl::substituteTypes(
    const std::unordered_map<std::string, Decl*>& subst) {
    if (pointeeType) {
        auto it = subst.find(pointeeType->name);
        if (it != subst.end())
            pointeeType = it->second;
    }
}

// ── FunctionDecl ──────────────────────────────────────────────────────────────

std::unique_ptr<Decl> FunctionDecl::clone() const {
    auto c = std::make_unique<FunctionDecl>();
    c->name          = name;
    c->modulePath    = modulePath;
    c->loc           = loc;
    c->dropTracked   = dropTracked;
    c->moveOnly      = moveOnly;
    c->returnType    = returnType;
    c->isVariadic    = isVariadic;
    c->isExtern      = isExtern;
    c->externABI     = externABI;
    c->genericParams = genericParams;
    for (const auto& p : params)
        c->params.push_back({p.name, p.type});
    return c;
}

void FunctionDecl::substituteTypes(
    const std::unordered_map<std::string, Decl*>& subst) {
    if (returnType) {
        auto it = subst.find(returnType->name);
        if (it != subst.end())
            returnType = it->second;
    }
    for (auto& p : params) {
        if (!p.type) continue;
        auto it = subst.find(p.type->name);
        if (it != subst.end())
            p.type = it->second;
    }
}

// ── ExtendedDecl ──────────────────────────────────────────────────────────────

std::unique_ptr<Decl> ExtendedDecl::clone() const {
    auto c = std::make_unique<ExtendedDecl>();
    c->name          = name;
    c->modulePath    = modulePath;
    c->loc           = loc;
    c->dropTracked   = dropTracked;
    c->moveOnly      = moveOnly;
    c->extendedKind  = extendedKind;
    c->elementType   = elementType;
    c->keyType       = keyType;
    c->valueType     = valueType;
    c->cPrefix       = cPrefix;
    return c;
}

void ExtendedDecl::substituteTypes(
    const std::unordered_map<std::string, Decl*>& subst) {
    if (elementType) {
        auto it = subst.find(elementType->name);
        if (it != subst.end()) elementType = it->second;
    }
    if (keyType) {
        auto it = subst.find(keyType->name);
        if (it != subst.end()) keyType = it->second;
    }
    if (valueType) {
        auto it = subst.find(valueType->name);
        if (it != subst.end()) valueType = it->second;
    }
}

// ── TupleDecl ─────────────────────────────────────────────────────────────────

std::unique_ptr<Decl> TupleDecl::clone() const {
    auto c = std::make_unique<TupleDecl>();
    c->name          = name;
    c->modulePath    = modulePath;
    c->loc           = loc;
    c->dropTracked   = dropTracked;
    c->moveOnly      = moveOnly;
    c->elements      = elements; // shallow copy of pointers
    return c;
}

void TupleDecl::substituteTypes(
    const std::unordered_map<std::string, Decl*>& subst) {
    for (auto& elem : elements) {
        if (!elem) continue;
        auto it = subst.find(elem->name);
        if (it != subst.end())
            elem = it->second;
    }
}

// ── TypeAliasDecl ─────────────────────────────────────────────────────────────

std::unique_ptr<Decl> TypeAliasDecl::clone() const {
    auto c = std::make_unique<TypeAliasDecl>();
    c->name          = name;
    c->modulePath    = modulePath;
    c->loc           = loc;
    c->dropTracked   = dropTracked;
    c->moveOnly      = moveOnly;
    c->target        = target;
    c->arraySize     = arraySize;
    c->arraySizes    = arraySizes;
    return c;
}

void TypeAliasDecl::substituteTypes(
    const std::unordered_map<std::string, Decl*>& subst) {
    if (target) {
        auto it = subst.find(target->name);
        if (it != subst.end())
            target = it->second;
    }
}

// ── GenericTemplateDecl ───────────────────────────────────────────────────────

std::unique_ptr<Decl> GenericTemplateDecl::clone() const {
    auto c = std::make_unique<GenericTemplateDecl>();
    c->name          = name;
    c->modulePath    = modulePath;
    c->loc           = loc;
    c->dropTracked   = dropTracked;
    c->moveOnly      = moveOnly;
    c->typeParams    = typeParams;
    c->pattern       = pattern ? pattern->clone() : nullptr;
    return c;
}

void GenericTemplateDecl::substituteTypes(
    const std::unordered_map<std::string, Decl*>& subst) {
    // Templates themselves are never substituted — they are the pattern.
    // Instantiation clones the pattern and substitutes the clone.
}

// ── IntrinsicDecl ─────────────────────────────────────────────────────────────

std::unique_ptr<Decl> IntrinsicDecl::clone() const {
    auto c = std::make_unique<IntrinsicDecl>();
    c->name           = name;
    c->modulePath     = modulePath;
    c->loc            = loc;
    c->returnType     = returnType;
    c->lowering       = lowering;
    c->llvmIntrinsic  = llvmIntrinsic;
    c->builtinName    = builtinName;
    for (const auto& p : params)
        c->params.push_back({p.name, p.type});
    return c;
}

} // namespace semantic
