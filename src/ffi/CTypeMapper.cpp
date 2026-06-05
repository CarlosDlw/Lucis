#include "ffi/CTypeMapper.h"

CTypeMapper::CTypeMapper(TypeRegistry& registry, CBindings& bindings)
    : registry_(registry), bindings_(bindings) {}

const TypeInfo* CTypeMapper::map(CXType cxType) {
    // Peel through typedefs to the canonical type
    CXType canonical = clang_getCanonicalType(cxType);

    switch (canonical.kind) {
    case CXType_Void:
        return registry_.lookup("void");

    case CXType_Bool:
        return registry_.lookup("bool");

    case CXType_Char_S:
    case CXType_Char_U:
    case CXType_SChar:
        return registry_.lookup("char");

    case CXType_UChar:
        return registry_.lookup("uint8");

    case CXType_Short:
        return registry_.lookup("int16");
    case CXType_UShort:
        return registry_.lookup("uint16");

    case CXType_Int:
        return registry_.lookup("int32");
    case CXType_UInt:
        return registry_.lookup("uint32");

    case CXType_Long:
        return registry_.lookup("int64");
    case CXType_ULong:
        return registry_.lookup("uint64");

    case CXType_LongLong:
        return registry_.lookup("int64");
    case CXType_ULongLong:
        return registry_.lookup("uint64");

    case CXType_Int128:
        return registry_.lookup("int128");
    case CXType_UInt128:
        return registry_.lookup("uint128");

    case CXType_Float:
        return registry_.lookup("float32");

    case CXType_Double:
        return registry_.lookup("float64");

    case CXType_LongDouble:
        return registry_.lookup("float80");

    case CXType_Pointer:
        return mapPointer(canonical);

    case CXType_FunctionProto:
    case CXType_FunctionNoProto:
        return mapFunctionProto(canonical);

    case CXType_IncompleteArray:
    case CXType_ConstantArray:
        // Arrays in C function params decay to pointers
        return mapPointer(canonical);

    case CXType_Record: {
        // Struct or union from C header — look up by name
        CXString cxName = clang_getTypeSpelling(canonical);
        std::string fullName = clang_getCString(cxName);
        clang_disposeString(cxName);

        // Strip "struct " / "union " prefix if present
        std::string name = fullName;
        if (name.substr(0, 7) == "struct ") name = name.substr(7);
        else if (name.substr(0, 6) == "union ")  name = name.substr(6);

        // Check if already registered (in registry or bindings)
        if (auto* ti = registry_.lookup(name)) return ti;

        // Try to map via cursor
        CXCursor declCursor = clang_getTypeDeclaration(canonical);
        if (clang_Cursor_isNull(declCursor)) return nullptr;
        return mapStruct(declCursor);
    }

    case CXType_Enum: {
        // C enums are integer types — map to the underlying integer type,
        // or fall back to int32 (the C default for enums).
        CXType underlying = clang_getEnumDeclIntegerType(
            clang_getTypeDeclaration(canonical));
        if (underlying.kind != CXType_Invalid) {
            auto* ti = map(underlying);
            if (ti) return ti;
        }
        return registry_.lookup("int32");
    }

    default:
        return nullptr;
    }
}

const TypeInfo* CTypeMapper::mapPointer(CXType cxType) {
    if (cxType.kind == CXType_IncompleteArray || cxType.kind == CXType_ConstantArray) {
        CXType elemType = clang_getArrayElementType(cxType);
        auto* elemTI = map(elemType);
        if (!elemTI) return nullptr;
        return makePointerTo(elemTI);
    }

    CXType pointee = clang_getPointeeType(cxType);

    // char* → *char
    if (pointee.kind == CXType_Char_S || pointee.kind == CXType_Char_U ||
        pointee.kind == CXType_SChar) {
        auto* charTI = registry_.lookup("char");
        return makePointerTo(charTI);
    }

    // void* → *void
    if (pointee.kind == CXType_Void) {
        auto* voidTI = registry_.lookup("void");
        return makePointerTo(voidTI);
    }

    // Recurse for other pointer types
    auto* pointeeTI = map(pointee);
    if (!pointeeTI) {
        // Unknown pointee → treat as *void
        auto* voidTI = registry_.lookup("void");
        return makePointerTo(voidTI);
    }
    return makePointerTo(pointeeTI);
}

const TypeInfo* CTypeMapper::mapFunctionProto(CXType cxType) {
    auto* retTI = map(clang_getResultType(cxType));
    if (!retTI) retTI = registry_.lookup("void");

    int numArgs = clang_getNumArgTypes(cxType);
    std::vector<const TypeInfo*> paramTypes;
    for (int i = 0; i < numArgs; i++) {
        auto* pTI = map(clang_getArgType(cxType, i));
        if (!pTI) return nullptr;
        paramTypes.push_back(pTI);
    }

    bool isVariadic = (clang_isFunctionTypeVariadic(cxType) != 0);

    auto ti = std::make_unique<TypeInfo>();
    ti->kind = TypeKind::Function;
    ti->bitWidth = 0;
    ti->isSigned = false;
    ti->builtinSuffix = "ptr";
    ti->returnType = retTI;
    ti->paramTypes = paramTypes;
    ti->isVariadic = isVariadic;

    ti->name = "fn(";
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (i > 0) ti->name += ",";
        ti->name += paramTypes[i]->name;
    }
    if (isVariadic) {
        if (!paramTypes.empty()) ti->name += ",";
        ti->name += "...";
    }
    ti->name += ")->" + retTI->name;

    return bindings_.internType(std::move(ti));
}

const TypeInfo* CTypeMapper::makePointerTo(const TypeInfo* pointee) {
    auto ti = std::make_unique<TypeInfo>();
    ti->kind = TypeKind::Pointer;
    ti->name = "*" + pointee->name;
    ti->bitWidth = 0;
    ti->isSigned = false;
    ti->builtinSuffix = "ptr";
    ti->pointeeType = pointee;
    return bindings_.internType(std::move(ti));
}

const TypeInfo* CTypeMapper::makeArrayOf(const TypeInfo* element, unsigned size) {
    auto ti = std::make_unique<TypeInfo>();
    *ti = *element;                          // copy element properties
    ti->name = "[" + std::to_string(size) + "]" + element->name;
    ti->arraySize = size;
    ti->elementType = element;
    return bindings_.internType(std::move(ti));
}

const TypeInfo* CTypeMapper::mapStruct(CXCursor structCursor) {
    CXString cxName = clang_getCursorSpelling(structCursor);
    std::string name = clang_getCString(cxName);
    clang_disposeString(cxName);

    // Anonymous structs — skip
    if (name.empty()) return nullptr;

    // Already registered (either fully in bindings or skeleton in registry)
    if (auto* existing = registry_.lookup(name)) return existing;

    // Incomplete struct (forward decl with no definition) — register as opaque
    if (clang_isCursorDefinition(structCursor) == 0) {
        // Register a minimal TypeInfo so pointers to it resolve
        auto ti = std::make_unique<TypeInfo>();
        ti->name = name;
        ti->kind = TypeKind::Struct;
        ti->bitWidth = 0;
        ti->isSigned = false;
        auto* raw = bindings_.internType(std::move(ti));
        registry_.registerType(*raw);
        return registry_.lookup(name);
    }

    bool isUnion = (clang_getCursorKind(structCursor) == CXCursor_UnionDecl);

    // Register skeleton first to break circular references (e.g. *Node in Node)
    {
        TypeInfo skeleton;
        skeleton.name = name;
        skeleton.kind = TypeKind::Struct;
        skeleton.bitWidth = 0;
        skeleton.isSigned = false;
        registry_.registerType(skeleton);
    }

    // For unions: use byte-size representation for codegen (overlapping fields),
    // but still extract field metadata so the checker can resolve field access
    // like `event.type`.
    if (isUnion) {
        long long byteSize = clang_Type_getSizeOf(
            clang_getCursorType(structCursor));

        // Extract fields for name/type resolution
        std::vector<FieldInfo> fields;
        struct UnionFieldData {
            CTypeMapper* mapper;
            std::vector<FieldInfo>* fields;
        };
        UnionFieldData ufd = { this, &fields };
        clang_visitChildren(structCursor,
            [](CXCursor child, CXCursor /*parent*/, CXClientData data)
                -> CXChildVisitResult {
                auto* ufd = static_cast<UnionFieldData*>(data);
                if (clang_getCursorKind(child) != CXCursor_FieldDecl)
                    return CXChildVisit_Continue;
                CXType fieldType = clang_getCursorType(child);
                CXString cxFieldName = clang_getCursorSpelling(child);
                std::string fieldName = clang_getCString(cxFieldName);
                clang_disposeString(cxFieldName);
                const TypeInfo* fieldTI = ufd->mapper->map(fieldType);
                if (fieldTI)
                    ufd->fields->push_back({fieldName, fieldTI});
                return CXChildVisit_Continue;
            },
            &ufd
        );

        TypeInfo ti;
        ti.name = name;
        ti.kind = TypeKind::Struct;
        ti.bitWidth = (byteSize > 0)
                          ? static_cast<unsigned>(byteSize) : 0;
        ti.isSigned = false;
        registry_.registerType(std::move(ti));

        CStruct cs;
        cs.name = name;
        cs.byteSize = (byteSize > 0)
                          ? static_cast<unsigned>(byteSize) : 0;
        cs.fields = std::move(fields);
        bindings_.addStruct(std::move(cs));

        return registry_.lookup(name);
    }

    // Collect fields
    std::vector<FieldInfo> fields;
    bool allMapped = true;
    bool needsOpaque = false; // bitfields or inline arrays require byte-size fallback

    struct FieldVisitorData {
        CTypeMapper* mapper;
        std::vector<FieldInfo>* fields;
        bool* allMapped;
        bool* needsOpaque;
    };

    FieldVisitorData fvd = { this, &fields, &allMapped, &needsOpaque };

    clang_visitChildren(structCursor,
        [](CXCursor child, CXCursor /*parent*/, CXClientData data)
            -> CXChildVisitResult {
            auto* fvd = static_cast<FieldVisitorData*>(data);
            if (clang_getCursorKind(child) != CXCursor_FieldDecl)
                return CXChildVisit_Continue;

            // Detect bitfields (packed fields that LLVM can't represent
            // as individual struct members with correct layout)
            if (clang_Cursor_isBitField(child))
                *fvd->needsOpaque = true;

            CXType fieldType = clang_getCursorType(child);
            CXType canonical = clang_getCanonicalType(fieldType);

            CXString cxFieldName = clang_getCursorSpelling(child);
            std::string fieldName = clang_getCString(cxFieldName);
            clang_disposeString(cxFieldName);

            const TypeInfo* fieldTI = nullptr;

            // Inline fixed-size arrays (e.g. uint32_t chars[6]):
            // map() would produce a pointer, but the memory is inline in the
            // struct — use makeArrayOf to preserve the correct layout.
            if (canonical.kind == CXType_ConstantArray) {
                CXType elemCXType = clang_getArrayElementType(canonical);
                long long arrSize = clang_getNumElements(canonical);
                auto* elemTI = fvd->mapper->map(elemCXType);
                if (!elemTI || arrSize <= 0) {
                    *fvd->allMapped = false;
                    return CXChildVisit_Break;
                }
                fieldTI = fvd->mapper->makeArrayOf(
                    elemTI, static_cast<unsigned>(arrSize));
            } else {
                fieldTI = fvd->mapper->map(fieldType);
            }

            if (!fieldTI) {
                *fvd->allMapped = false;
                return CXChildVisit_Break;
            }

            fvd->fields->push_back({ fieldName, fieldTI });
            return CXChildVisit_Continue;
        },
        &fvd
    );

    // If fields contain bitfields, inline arrays, or couldn't be resolved,
    // fall back to opaque byte-size representation for correct memory layout
    if (!allMapped || needsOpaque) {
        long long byteSize = clang_Type_getSizeOf(
            clang_getCursorType(structCursor));

        TypeInfo ti;
        ti.name = name;
        ti.kind = TypeKind::Struct;
        ti.bitWidth = (byteSize > 0)
                          ? static_cast<unsigned>(byteSize) : 0;
        ti.isSigned = false;
        registry_.registerType(std::move(ti));

        CStruct cs;
        cs.name = name;
        cs.byteSize = (byteSize > 0)
                          ? static_cast<unsigned>(byteSize) : 0;
        bindings_.addStruct(std::move(cs));

        return registry_.lookup(name);
    }

    // Build full TypeInfo
    TypeInfo ti;
    ti.name = name;
    ti.kind = TypeKind::Struct;
    ti.bitWidth = 0;
    ti.isSigned = false;
    ti.fields = fields;
    registry_.registerType(std::move(ti));

    // Register in CBindings
    CStruct cs;
    cs.name = name;
    cs.fields = std::move(fields);
    bindings_.addStruct(std::move(cs));

    return registry_.lookup(name);
}
