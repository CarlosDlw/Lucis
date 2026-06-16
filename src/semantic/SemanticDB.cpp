#include "semantic/SemanticDB.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace semantic {

// ── Internal helpers ──────────────────────────────────────────────────────────

static std::string kindToString(DeclKind k) {
    switch (k) {
    case DeclKind::Integer:  return "integer";
    case DeclKind::Float:    return "float";
    case DeclKind::Bool:     return "bool";
    case DeclKind::Char:     return "char";
    case DeclKind::Void:     return "void";
    case DeclKind::String:   return "string";
    case DeclKind::VAList:   return "va_list";
    case DeclKind::Struct:   return "struct";
    case DeclKind::Union:    return "union";
    case DeclKind::Enum:     return "enum";
    case DeclKind::Pointer:  return "pointer";
    case DeclKind::Function: return "function";
    case DeclKind::Extended: return "extended";
    case DeclKind::Tuple:    return "tuple";
    case DeclKind::TypeAlias:        return "type alias";
    case DeclKind::GenericTemplate:  return "generic template";
    case DeclKind::Intrinsic:        return "intrinsic";
    }
    return "unknown";
}

// ── Forward declarations ──────────────────────────────────────────────────────

bool SemanticDB::forwardDeclare(const std::string& name,
                                DeclKind kind,
                                const std::string& modulePath,
                                SourceLocation loc) {
    // Don't forward-declare if a full type already exists
    if (types_.count(name)) return false;

    auto it = forwardDecls_.find(name);
    if (it != forwardDecls_.end()) {
        // Already forward-declared — must be same kind
        if (it->second.kind != kind) return false;
        // Update location with the earlier one (keep first)
        return true;
    }

    forwardDecls_[name] = {kind, modulePath, loc};
    return true;
}

bool SemanticDB::hasForwardDecl(const std::string& name) const {
    return forwardDecls_.count(name) > 0 && types_.count(name) == 0;
}

DeclKind SemanticDB::forwardDeclKind(const std::string& name) const {
    auto it = forwardDecls_.find(name);
    if (it != forwardDecls_.end()) return it->second.kind;
    if (types_.count(name)) return types_.at(name)->kind;
    return DeclKind::Void; // fallback
}

// ── Type registration ─────────────────────────────────────────────────────────

void SemanticDB::flushPendingExtends(const std::string& structName,
                                     StructDecl* sd) {
    auto it = pendingExtends_.find(structName);
    if (it == pendingExtends_.end()) return;

    for (auto& m : it->second)
        sd->methods.push_back(std::move(m));
    pendingExtends_.erase(it);
}

bool SemanticDB::registerType(std::unique_ptr<Decl> decl) {
    if (!decl) return false;

    const std::string& name = decl->name;

    // Check if a full type already exists
    if (types_.count(name)) {
        // Duplicate registration — reject unless it's a forward-decl skeleton
        // being upgraded (handled by the caller before calling registerType).
        return false;
    }

    // Flush pending extend methods for structs
    if (decl->kind == DeclKind::Struct) {
        auto* sd = static_cast<StructDecl*>(decl.get());
        flushPendingExtends(name, sd);
    }

    // Remove any forward declaration (it's now fulfilled)
    forwardDecls_.erase(name);

    // Index in module
    if (!decl->modulePath.empty())
        indexInModule(decl->modulePath, name, decl.get());

    Decl* ptr = decl.get();
    types_[name] = std::move(decl);

    // If this is a struct, update extend method param/return types that were
    // registered before the type existed (they point to forward-decl placeholders).
    // No-op for now — the Checker registers types in the correct order.

    return true;
}

// ── Function registration ─────────────────────────────────────────────────────

bool SemanticDB::registerFunction(std::unique_ptr<FunctionDecl> decl) {
    if (!decl) return false;
    const std::string& name = decl->name;

    if (functions_.count(name)) return false;

    if (!decl->modulePath.empty())
        indexInModule(decl->modulePath, name, decl.get());

    functions_[name] = std::move(decl);
    return true;
}

// ── Generic template registration ─────────────────────────────────────────────

bool SemanticDB::registerGeneric(std::unique_ptr<GenericTemplateDecl> tmpl) {
    if (!tmpl) return false;
    const std::string& name = tmpl->name;

    if (generics_.count(name)) return false;

    if (!tmpl->modulePath.empty())
        indexInModule(tmpl->modulePath, name, tmpl.get());

    generics_[name] = std::move(tmpl);
    return true;
}

// ── Intrinsic registration ────────────────────────────────────────────────────

bool SemanticDB::registerIntrinsic(std::unique_ptr<IntrinsicDecl> decl) {
    if (!decl) return false;
    const std::string& name = decl->name;

    if (intrinsics_.count(name)) return false;

    intrinsics_[name] = std::move(decl);
    return true;
}

// ── Builtin registration ──────────────────────────────────────────────────────

bool SemanticDB::registerBuiltin(std::unique_ptr<Decl> decl) {
    if (!decl) return false;
    const std::string& name = decl->name;

    if (builtins_.count(name)) return false;

    builtins_[name] = std::move(decl);
    return true;
}

// ── Extend methods ────────────────────────────────────────────────────────────

bool SemanticDB::mergeExtendMethods(const std::string& structName,
                                    std::vector<MethodInfo> methods) {
    // If struct is already fully registered, append directly
    auto it = types_.find(structName);
    if (it != types_.end()) {
        if (it->second->kind != DeclKind::Struct) return false;
        auto* sd = static_cast<StructDecl*>(it->second.get());
        for (auto& m : methods)
            sd->methods.push_back(std::move(m));
        return true;
    }

    // Otherwise, queue for later (struct might be forward-declared)
    auto& pending = pendingExtends_[structName];
    for (auto& m : methods)
        pending.push_back(std::move(m));
    return true;
}

// ── Lookup ────────────────────────────────────────────────────────────────────

const Decl* SemanticDB::lookupAny(const std::string& name) const {
    // Priority: types > functions > builtins > intrinsics > generics
    if (auto it = types_.find(name); it != types_.end())
        return it->second.get();
    if (auto it = functions_.find(name); it != functions_.end())
        return it->second.get();
    if (auto it = builtins_.find(name); it != builtins_.end())
        return it->second.get();
    if (auto it = intrinsics_.find(name); it != intrinsics_.end())
        return it->second.get();
    if (auto it = generics_.find(name); it != generics_.end())
        return it->second.get();
    return nullptr;
}

const FunctionDecl* SemanticDB::lookupFunction(const std::string& name) const {
    auto it = functions_.find(name);
    return it != functions_.end() ? it->second.get() : nullptr;
}

const GenericTemplateDecl* SemanticDB::lookupGeneric(
    const std::string& name) const {
    auto it = generics_.find(name);
    return it != generics_.end() ? it->second.get() : nullptr;
}

const IntrinsicDecl* SemanticDB::lookupIntrinsic(
    const std::string& name) const {
    auto it = intrinsics_.find(name);
    return it != intrinsics_.end() ? it->second.get() : nullptr;
}

// ── Cross-file lookup ─────────────────────────────────────────────────────────

const Decl* SemanticDB::findInModule(const std::string& modulePath,
                                     const std::string& name) const {
    auto mit = modules_.find(modulePath);
    if (mit == modules_.end()) return nullptr;
    auto sit = mit->second.find(name);
    return sit != mit->second.end() ? sit->second : nullptr;
}

// ── Existence checks ──────────────────────────────────────────────────────────

bool SemanticDB::hasType(const std::string& name) const {
    return types_.count(name) > 0;
}
bool SemanticDB::hasFunction(const std::string& name) const {
    return functions_.count(name) > 0;
}
bool SemanticDB::hasGeneric(const std::string& name) const {
    return generics_.count(name) > 0;
}
bool SemanticDB::hasIntrinsic(const std::string& name) const {
    return intrinsics_.count(name) > 0;
}
bool SemanticDB::hasBuiltin(const std::string& name) const {
    return builtins_.count(name) > 0;
}

// ── Module index ──────────────────────────────────────────────────────────────

void SemanticDB::indexInModule(const std::string& modulePath,
                                const std::string& name, Decl* ptr) {
    if (modulePath.empty()) return;
    modules_[modulePath][name] = ptr;
}

// ── Generic instantiation ─────────────────────────────────────────────────────

std::string SemanticDB::mangleGenericName(
    const std::string& baseName,
    const std::vector<Decl*>& typeArgs) {
    if (typeArgs.empty()) return baseName;

    std::ostringstream oss;
    oss << baseName;
    for (const auto* arg : typeArgs) {
        oss << "__";
        if (arg) {
            // Replace '*' in pointer names (e.g. *int32 → P_int32)
            std::string argName = arg->name;
            for (auto& c : argName)
                if (c == '*') c = 'P';
            oss << argName;
        }
    }
    return oss.str();
}

const Decl* SemanticDB::instantiateGeneric(
    const std::string& baseName,
    const std::vector<Decl*>& typeArgs,
    SourceLocation usageLoc) {
    // Look up the generic template
    auto* tmpl = lookupGeneric(baseName);
    if (!tmpl) return nullptr;

    // Validate argument count
    if (typeArgs.size() != tmpl->typeParams.size()) return nullptr;

    // Mangle the name and check for existing instantiation
    std::string mangled = mangleGenericName(baseName, typeArgs);
    if (auto it = types_.find(mangled); it != types_.end())
        return it->second.get();

    // Cycle detection
    if (!instantiating_.insert(mangled).second)
        return nullptr; // cycle detected

    // Clone the pattern
    auto concrete = tmpl->pattern->clone();
    if (!concrete) {
        instantiating_.erase(mangled);
        return nullptr;
    }

    // Build substitution map: typeParamName → concrete Decl*
    std::unordered_map<std::string, Decl*> subst;
    for (size_t i = 0; i < tmpl->typeParams.size(); ++i) {
        subst[tmpl->typeParams[i]] = typeArgs[i];
    }

    // Apply substitution
    concrete->substituteTypes(subst);

    // Set identity
    concrete->name       = mangled;
    concrete->modulePath = tmpl->modulePath;
    concrete->loc        = usageLoc;

    // If the pattern is a StructDecl, we also need to instantiate any
    // generic extend blocks associated with the base name.  This is done
    // by the Checker calling mergeExtendMethods before instantiateGeneric,
    // so the methods are already in the pattern's methods list.

    Decl* resultPtr = concrete.get();
    types_[mangled] = std::move(concrete);

    // Index in module
    if (!resultPtr->modulePath.empty())
        indexInModule(resultPtr->modulePath, mangled, resultPtr);

    instantiating_.erase(mangled);
    return resultPtr;
}

// ── Iteration ─────────────────────────────────────────────────────────────────

std::vector<const Decl*> SemanticDB::allTypes() const {
    std::vector<const Decl*> result;
    result.reserve(types_.size() + builtins_.size());
    for (const auto& [_, decl] : types_)
        result.push_back(decl.get());
    for (const auto& [_, decl] : builtins_)
        result.push_back(decl.get());
    return result;
}

std::vector<const FunctionDecl*> SemanticDB::allFunctions() const {
    std::vector<const FunctionDecl*> result;
    result.reserve(functions_.size());
    for (const auto& [_, decl] : functions_)
        result.push_back(decl.get());
    return result;
}

std::vector<const GenericTemplateDecl*> SemanticDB::allGenerics() const {
    std::vector<const GenericTemplateDecl*> result;
    result.reserve(generics_.size());
    for (const auto& [_, decl] : generics_)
        result.push_back(decl.get());
    return result;
}

// ── Serialization ─────────────────────────────────────────────────────────────

// Format (line-based, one directive per line):
//   V 1                          — version header
//   B name kind bw sign suffix   — builtin primitive
//   X name kind cPrefix [elem key val] — builtin extended type
//   T name kind                  — type header
//     F fieldName typeName dims [size1 size2 ...] [auto]  — field
//     M methodName returnType static (paramType ...)       — method
//   E                            — end type block
//   C name returnType [variadic] (paramType ...)  — function
//   G name kind param1 param2 ...                 — generic template
//
// All type references are by name (resolved on load against builtins first).

static std::string declKindToStr(DeclKind k) {
    switch (k) {
    case DeclKind::Integer:  return "Integer";
    case DeclKind::Float:    return "Float";
    case DeclKind::Bool:     return "Bool";
    case DeclKind::Char:     return "Char";
    case DeclKind::Void:     return "Void";
    case DeclKind::String:   return "String";
    case DeclKind::VAList:   return "VAList";
    case DeclKind::Struct:   return "Struct";
    case DeclKind::Union:    return "Union";
    case DeclKind::Enum:     return "Enum";
    case DeclKind::Pointer:  return "Pointer";
    case DeclKind::Function: return "Function";
    case DeclKind::Extended: return "Extended";
    case DeclKind::Tuple:    return "Tuple";
    case DeclKind::TypeAlias:       return "TypeAlias";
    case DeclKind::GenericTemplate: return "GenericTemplate";
    case DeclKind::Intrinsic:       return "Intrinsic";
    }
    return "Void";
}

static DeclKind strToDeclKind(const std::string& s) {
    if (s == "Integer")  return DeclKind::Integer;
    if (s == "Float")    return DeclKind::Float;
    if (s == "Bool")     return DeclKind::Bool;
    if (s == "Char")     return DeclKind::Char;
    if (s == "Void")     return DeclKind::Void;
    if (s == "String")   return DeclKind::String;
    if (s == "VAList")   return DeclKind::VAList;
    if (s == "Struct")   return DeclKind::Struct;
    if (s == "Union")    return DeclKind::Union;
    if (s == "Enum")     return DeclKind::Enum;
    if (s == "Pointer")  return DeclKind::Pointer;
    if (s == "Function") return DeclKind::Function;
    if (s == "Extended") return DeclKind::Extended;
    if (s == "Tuple")    return DeclKind::Tuple;
    if (s == "TypeAlias")       return DeclKind::TypeAlias;
    if (s == "GenericTemplate") return DeclKind::GenericTemplate;
    if (s == "Intrinsic")       return DeclKind::Intrinsic;
    return DeclKind::Void;
}

static std::string variantPayloadKindToStr(VariantPayloadKind k) {
    switch (k) {
    case VariantPayloadKind::Unit:  return "Unit";
    case VariantPayloadKind::Tuple: return "Tuple";
    case VariantPayloadKind::Named: return "Named";
    }
    return "Unit";
}

static VariantPayloadKind strToVariantPayloadKind(const std::string& s) {
    if (s == "Tuple") return VariantPayloadKind::Tuple;
    if (s == "Named") return VariantPayloadKind::Named;
    return VariantPayloadKind::Unit;
}

void SemanticDB::save(const std::string& path) const {
    std::ofstream out(path);
    if (!out) return;

    out << "VER 1\n";

    // ── Builtins ──────────────────────────────────────────────────────────
    for (const auto& [name, decl] : builtins_) {
        if (auto* pd = decl->as<PrimitiveDecl>()) {
            out << "B " << name << " " << declKindToStr(pd->kind) << " "
                << pd->bitWidth << " " << (pd->isSigned ? "1" : "0") << " \""
                << pd->builtinSuffix << "\"\n";
        } else if (auto* ed = decl->as<ExtendedDecl>()) {
            out << "X " << name << " " << ed->extendedKind << " "
                << ed->cPrefix << " "
                << (ed->elementType ? ed->elementType->name : "-") << " "
                << (ed->keyType ? ed->keyType->name : "-") << " "
                << (ed->valueType ? ed->valueType->name : "-") << "\n";
        } else if (auto* sd = decl->as<StructDecl>()) {
            out << "T " << name << " Struct\n";
            for (const auto& f : sd->fields) {
                out << "  F " << f.name << " "
                    << (f.type ? f.type->name : "void") << " "
                    << f.arrayDims;
                for (auto sz : f.arraySizes) out << " " << sz;
                if (f.autoFill) out << " auto";
                out << "\n";
            }
            out << "E\n";
        }
    }

    // ── Types ─────────────────────────────────────────────────────────────
    for (const auto& [name, decl] : types_) {
        if (auto* sd = decl->as<StructDecl>()) {
            out << "T " << name << " Struct\n";
            for (const auto& f : sd->fields) {
                out << "  F " << f.name << " "
                    << (f.type ? f.type->name : "void") << " "
                    << f.arrayDims;
                for (auto sz : f.arraySizes) out << " " << sz;
                if (f.autoFill) out << " auto";
                out << "\n";
            }
            for (const auto& m : sd->methods) {
                out << "  M " << m.name << " "
                    << (m.returnType ? m.returnType->name : "void") << " "
                    << (m.isStatic ? "static" : "instance");
                for (const auto& p : m.params)
                    out << " " << (p.type ? p.type->name : "void");
                out << "\n";
            }
            out << "E\n";
        } else if (auto* ud = decl->as<UnionDecl>()) {
            out << "T " << name << " Union\n";
            for (const auto& f : ud->fields) {
                out << "  F " << f.name << " "
                    << (f.type ? f.type->name : "void") << " "
                    << f.arrayDims;
                for (auto sz : f.arraySizes) out << " " << sz;
                out << "\n";
            }
            out << "E\n";
        } else if (auto* ed = decl->as<EnumDecl>()) {
            out << "T " << name << " Enum\n";
            for (const auto& v : ed->variants) {
                out << "  V " << v.name << " " << v.discriminant << " "
                    << variantPayloadKindToStr(v.payloadKind) << "\n";
                for (const auto& pf : v.payloadFields) {
                    out << "    F " << pf.name << " "
                        << (pf.type ? pf.type->name : "void") << " "
                        << pf.arrayDims;
                    for (auto sz : pf.arraySizes) out << " " << sz;
                    out << "\n";
                }
            }
            out << "E\n";
        } else if (auto* pd = decl->as<PointerDecl>()) {
            out << "T " << name << " Pointer "
                << (pd->pointeeType ? pd->pointeeType->name : "void") << "\n";
            out << "E\n";
        } else if (auto* td = decl->as<TupleDecl>()) {
            out << "T " << name << " Tuple";
            for (auto* e : td->elements)
                out << " " << (e ? e->name : "void");
            out << "\nE\n";
        } else if (auto* ta = decl->as<TypeAliasDecl>()) {
            out << "T " << name << " TypeAlias "
                << (ta->target ? ta->target->name : "void") << " "
                << ta->arraySize;
            for (auto sz : ta->arraySizes) out << " " << sz;
            out << "\nE\n";
        }
    }

    // ── Functions ─────────────────────────────────────────────────────────
    for (const auto& [name, decl] : functions_) {
        out << "C " << name << " "
            << (decl->returnType ? decl->returnType->name : "void")
            << (decl->isVariadic ? " variadic" : "")
            << (decl->isExtern ? " extern" : "");
        if (!decl->externABI.empty()) out << " " << decl->externABI;
        for (const auto& p : decl->params)
            out << " " << (p.type ? p.type->name : "void");
        out << "\n";
    }

    // ── Generics ──────────────────────────────────────────────────────────
    for (const auto& [name, decl] : generics_) {
        out << "G " << name << " "
            << declKindToStr(decl->pattern ? decl->pattern->kind
                                           : DeclKind::Void);
        for (const auto& tp : decl->typeParams)
            out << " " << tp;
        out << "\n";
    }

    out.close();
}

std::unique_ptr<SemanticDB> SemanticDB::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return nullptr;

    auto db = std::make_unique<SemanticDB>();

    std::string line;
    std::string currentTypeName, currentTypeKind;
    std::unique_ptr<StructDecl> currentStruct;
    std::unique_ptr<UnionDecl> currentUnion;
    std::unique_ptr<EnumDecl> currentEnum;
    std::unique_ptr<PointerDecl> currentPointer;
    std::unique_ptr<TupleDecl> currentTuple;
    std::unique_ptr<TypeAliasDecl> currentAlias;

    auto flushCurrentType = [&]() {
        if (currentStruct)      db->registerType(std::move(currentStruct));
        else if (currentUnion)  db->registerType(std::move(currentUnion));
        else if (currentEnum)   db->registerType(std::move(currentEnum));
        else if (currentPointer) db->registerType(std::move(currentPointer));
        else if (currentTuple)  db->registerType(std::move(currentTuple));
        else if (currentAlias)  db->registerType(std::move(currentAlias));
    };

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string directive;
        ss >> directive;

        if (directive == "VER") {
            int ver; ss >> ver; // version, ignored for now
        }
        else if (directive == "B") {
            std::string name, kindStr, suffix;
            unsigned bw = 0;
            int signd = 0;
            ss >> name >> kindStr >> bw >> signd;
            // Read quoted suffix
            char q; ss >> q; // skip opening quote
            std::getline(ss, suffix, '"');

            auto decl = std::make_unique<PrimitiveDecl>(strToDeclKind(kindStr));
            decl->name = name;
            decl->bitWidth = bw;
            decl->isSigned = signd != 0;
            decl->builtinSuffix = suffix;
            db->registerBuiltin(std::move(decl));
        }
        else if (directive == "X") {
            std::string name, extKind, cPrefix, elem, key, val;
            ss >> name >> extKind >> cPrefix >> elem >> key >> val;

            auto decl = std::make_unique<ExtendedDecl>();
            decl->name = name;
            decl->extendedKind = extKind;
            decl->cPrefix = cPrefix;
            // Type refs resolved lazily — store names for now
            (void)elem; (void)key; (void)val;
            db->registerBuiltin(std::move(decl));
        }
        else if (directive == "T") {
            flushCurrentType();

            ss >> currentTypeName >> currentTypeKind;
            currentStruct.reset();
            currentUnion.reset();
            currentEnum.reset();
            currentPointer.reset();
            currentTuple.reset();
            currentAlias.reset();

            if (currentTypeKind == "Struct") {
                currentStruct = std::make_unique<StructDecl>();
                currentStruct->name = currentTypeName;
            } else if (currentTypeKind == "Union") {
                currentUnion = std::make_unique<UnionDecl>();
                currentUnion->name = currentTypeName;
            } else if (currentTypeKind == "Enum") {
                currentEnum = std::make_unique<EnumDecl>();
                currentEnum->name = currentTypeName;
                currentEnum->bitWidth = 32;
            } else if (currentTypeKind == "Pointer") {
                currentPointer = std::make_unique<PointerDecl>();
                currentPointer->name = currentTypeName;
                std::string pointeeName; ss >> pointeeName;
            } else if (currentTypeKind == "Tuple") {
                currentTuple = std::make_unique<TupleDecl>();
                currentTuple->name = currentTypeName;
            } else if (currentTypeKind == "TypeAlias") {
                currentAlias = std::make_unique<TypeAliasDecl>();
                currentAlias->name = currentTypeName;
                std::string targetName; ss >> targetName;
            }
        }
        else if (directive == "F") {
            std::string fieldName, typeName;
            unsigned dims = 0;
            ss >> fieldName >> typeName >> dims;

            FieldInfo fi;
            fi.name = fieldName;
            fi.arrayDims = dims;

            // Read array sizes
            std::string token;
            while (ss >> token) {
                if (token == "auto") {
                    fi.autoFill = true;
                } else if (token == "E") {
                    break;
                } else {
                    try {
                        fi.arraySizes.push_back(
                            static_cast<unsigned>(std::stoul(token)));
                    } catch (...) {}
                }
            }

            if (currentStruct) {
                currentStruct->fields.push_back(std::move(fi));
            } else if (currentUnion) {
                currentUnion->fields.push_back(std::move(fi));
            } else if (currentEnum && !currentEnum->variants.empty()) {
                currentEnum->variants.back().payloadFields.push_back(std::move(fi));
            }
        }
        else if (directive == "M") {
            std::string methodName, returnType, isStaticStr;
            ss >> methodName >> returnType >> isStaticStr;

            MethodInfo mi;
            mi.name = methodName;
            mi.isStatic = (isStaticStr == "static");

            std::string paramType;
            while (ss >> paramType) {
                ParamInfo pi;
                pi.type = nullptr; // resolved on next load
                mi.params.push_back(pi);
            }

            if (currentStruct)
                currentStruct->methods.push_back(std::move(mi));
        }
        else if (directive == "V" && currentEnum) {
            // Enum variant
            std::string varName, payloadKindStr;
            unsigned disc = 0;
            ss >> varName >> disc >> payloadKindStr;

            VariantInfo vi;
            vi.name = varName;
            vi.discriminant = disc;
            vi.payloadKind = strToVariantPayloadKind(payloadKindStr);
            currentEnum->variants.push_back(std::move(vi));
        }
        else if (directive == "E") {
            // End of type block — flushed at next T or end of file
        }
        else if (directive == "C") {
            std::string funcName, returnType, flag;
            ss >> funcName >> returnType;

            auto fd = std::make_unique<FunctionDecl>();
            fd->name = funcName;

            std::string param;
            while (ss >> param) {
                if (param == "variadic") {
                    fd->isVariadic = true;
                } else if (param == "extern") {
                    fd->isExtern = true;
                } else if (param == "C") {
                    fd->externABI = "C";
                } else {
                    ParamInfo pi;
                    pi.type = nullptr;
                    fd->params.push_back(pi);
                }
            }
            db->registerFunction(std::move(fd));
        }
        else if (directive == "G") {
            std::string genName, patternKind;
            ss >> genName >> patternKind;

            auto tmpl = std::make_unique<GenericTemplateDecl>();
            tmpl->name = genName;

            std::string tp;
            while (ss >> tp)
                tmpl->typeParams.push_back(tp);

            // Create a minimal pattern so the generic can be re-instantiated
            DeclKind pk = strToDeclKind(patternKind);
            switch (pk) {
            case DeclKind::Struct:
                tmpl->pattern = std::make_unique<StructDecl>();
                break;
            case DeclKind::Union:
                tmpl->pattern = std::make_unique<UnionDecl>();
                break;
            case DeclKind::Enum:
                tmpl->pattern = std::make_unique<EnumDecl>();
                break;
            case DeclKind::Function:
                tmpl->pattern = std::make_unique<FunctionDecl>();
                break;
            default: break;
            }
            if (tmpl->pattern) {
                tmpl->pattern->name = genName;
            }
            db->registerGeneric(std::move(tmpl));
        }
    }

    // Flush last type
    flushCurrentType();

    return db;
}

// ── Destructor ────────────────────────────────────────────────────────────────

SemanticDB::~SemanticDB() = default;

} // namespace semantic
