#include "lsp/SignatureHelpProvider.h"
#include "lsp/ProjectContext.h"
#include "lsp/DocComment.h"
#include "parser/Parser.h"
#include "ffi/CHeaderResolver.h"
#include "namespace/ModuleRegistry.h"

#include <sstream>
#include <fstream>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

SignatureHelpProvider::SignatureHelpProvider() : intrinsicRegistry_(typeRegistry_) {}

// ═══════════════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════════════

static std::string normalizeExtBaseName(const std::string& name) {
    if (name == "vec") return "Vec";
    if (name == "map") return "Map";
    if (name == "set") return "Set";
    return name;
}

static std::string safeText(antlr4::tree::TerminalNode *n) {
    return n ? n->getText() : "";
}
static std::string safeText(antlr4::ParserRuleContext *ctx) {
    return ctx ? ctx->getText() : "";
}
template<typename T>
static std::string safeIdAt(T *ctx, size_t i) {
    if (!ctx) return "";
    if (i >= ctx->IDENTIFIER().size()) return "";
    auto *n = ctx->IDENTIFIER(i);
    return n ? n->getText() : "";
}

std::string SignatureHelpProvider::typeSpecToString(LucisParser::TypeSpecContext* ctx) {
    return ctx ? ctx->getText() : "void";
}

std::string SignatureHelpProvider::paramNameFromType(const std::string& type) {
    if (type == "usize" || type == "isize") return "index";
    if (type == "bool") return "flag";
    if (type == "string") return "str";
    if (type == "char") return "ch";
    if (type == "uint32" || type == "int32") return "n";
    if (type == "float64" || type == "float32") return "x";
    if (type == "void") return "";
    if (!type.empty() && type.front() == '[') return "arr";
    if (type.find('<') != std::string::npos) return "val";
    return "value";
}

std::string SignatureHelpProvider::resolveTypePlaceholder(
        const std::string& raw,
        const std::string& selfType,
        const std::string& elemType,
        const std::string& keyType,
        const std::string& valType) {
    if (raw == "_self") return selfType;
    if (raw == "_elem") return elemType;
    if (raw == "_key")  return keyType.empty()  ? raw : keyType;
    if (raw == "_val")  return valType.empty()  ? raw : valType;
    if (raw == "_vec_key") return keyType.empty() ? raw : "vec<" + keyType + ">";
    if (raw == "_vec_val") return valType.empty() ? raw : "vec<" + valType + ">";
    return raw;
}

// ═══════════════════════════════════════════════════════════════════════
//  Call-site analysis
// ═══════════════════════════════════════════════════════════════════════

std::optional<SignatureHelpProvider::CallSite>
SignatureHelpProvider::analyzeCallSite(const std::string& source,
                                       size_t line, size_t col) {
    // Convert 0-based line:col to an offset into source.
    size_t offset = 0;
    size_t currentLine = 0;
    for (size_t i = 0; i < source.size(); i++) {
        if (currentLine == line) {
            offset = i + col;
            break;
        }
        if (source[i] == '\n') currentLine++;
    }
    if (offset == 0 && line > 0) return std::nullopt;
    if (offset > source.size()) offset = source.size();

    // Walk backwards from cursor to find the matching '(' while counting commas.
    int depth = 0;       // nesting depth for (), [], {}
    size_t commas = 0;   // commas at depth 0 (our call level)
    bool inString = false;
    bool inChar = false;

    size_t openParen = std::string::npos;
    for (size_t i = offset; i > 0; ) {
        --i;
        char c = source[i];

        // Skip string/char literals (simplified — doesn't handle escape sequences
        // at boundaries, but sufficient for parameter counting).
        if (c == '"' && !inChar) {
            inString = !inString;
            continue;
        }
        if (c == '\'' && !inString) {
            inChar = !inChar;
            continue;
        }
        if (inString || inChar) continue;

        // Track nesting
        if (c == ')' || c == ']' || c == '}') {
            depth++;
            continue;
        }
        if (c == '(' || c == '[' || c == '{') {
            if (depth > 0) { depth--; continue; }
            if (c == '(') {
                openParen = i;
                break;
            }
            // '[' or '{' at depth 0 means we're not in a function call
            return std::nullopt;
        }

        if (c == ',' && depth == 0) commas++;
    }

    if (openParen == std::string::npos) return std::nullopt;

    // Extract function/method name from just before the '('.
    // Walk backwards over whitespace, then collect identifier characters.
    // Handle generic calls: max<int32>( → funcName = "max"
    size_t nameEnd = openParen;
    while (nameEnd > 0 && source[nameEnd - 1] == ' ') nameEnd--;

    // If preceded by '>', this is a generic call — skip back over <type args>
    bool isGenericCall = (nameEnd > 0 && source[nameEnd - 1] == '>');
    if (isGenericCall) {
        size_t pos = nameEnd - 1; // points at '>'
        int angleDepth = 1;
        while (pos > 0 && angleDepth > 0) {
            pos--;
            if (source[pos] == '>') angleDepth++;
            else if (source[pos] == '<') angleDepth--;
        }
        nameEnd = pos; // points at '<'
        while (nameEnd > 0 && source[nameEnd - 1] == ' ') nameEnd--;
    }

    size_t nameStart = nameEnd;
    while (nameStart > 0) {
        char c = source[nameStart - 1];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            nameStart--;
        } else {
            break;
        }
    }

    if (nameStart == nameEnd) return std::nullopt;

    std::string funcName = source.substr(nameStart, nameEnd - nameStart);

    // Check if it's a method call: look for '.' before the identifier.
    CallSite site;
    site.functionName = funcName;
    site.activeParam  = commas;
    site.isMethodCall = false;
    site.isStaticCall = false;

    size_t checkPos = nameStart;
    while (checkPos > 0 && source[checkPos - 1] == ' ') checkPos--;

    // Check for static call: Type::method(...) or Type<T>::method(...)
    if (checkPos >= 2 && source[checkPos - 1] == ':' && source[checkPos - 2] == ':') {
        site.isStaticCall = true;
        size_t scopeEnd = checkPos - 2;
        while (scopeEnd > 0 && source[scopeEnd - 1] == ' ') scopeEnd--;

        // Handle generic base type: Node<int32>::method
        if (scopeEnd > 0 && source[scopeEnd - 1] == '>') {
            size_t pos = scopeEnd - 1;
            int angleDepth = 1;
            while (pos > 0 && angleDepth > 0) {
                pos--;
                if (source[pos] == '>') angleDepth++;
                else if (source[pos] == '<') angleDepth--;
            }
            // pos now points at '<'; walk back over base name
            size_t baseEnd = pos;
            while (baseEnd > 0 && source[baseEnd - 1] == ' ') baseEnd--;
            size_t baseStart = baseEnd;
            while (baseStart > 0) {
                char sc = source[baseStart - 1];
                if (std::isalnum(static_cast<unsigned char>(sc)) || sc == '_') {
                    baseStart--;
                } else {
                    break;
                }
            }
            if (baseStart < baseEnd)
                site.staticTypeName = source.substr(baseStart, baseEnd - baseStart);
        } else {
            size_t scopeStart = scopeEnd;
            while (scopeStart > 0) {
                char sc = source[scopeStart - 1];
                if (std::isalnum(static_cast<unsigned char>(sc)) || sc == '_') {
                    scopeStart--;
                } else {
                    break;
                }
            }
            if (scopeStart < scopeEnd)
                site.staticTypeName = source.substr(scopeStart, scopeEnd - scopeStart);
        }
    }
    // Check for method call: obj.method(...)
    else if (checkPos > 0 && source[checkPos - 1] == '.') {
        site.isMethodCall = true;
    }

    return site;
}

// ═══════════════════════════════════════════════════════════════════════
//  Signature builders
// ═══════════════════════════════════════════════════════════════════════

SignatureInfo SignatureHelpProvider::buildFromFunction(
        LucisParser::FunctionDeclContext* func,
        const std::vector<DocComment>& docs) {
    SignatureInfo sig;

    std::string retType = typeSpecToString(func->typeSpec());
    std::string name = safeIdAt(func, 0);

    std::ostringstream label;
    label << "fn " << name;

    // Show type parameters
    if (auto* tpl = func->typeParamList()) {
        label << "<";
        bool first = true;
        for (auto* tp : tpl->typeParam()) {
            if (!first) label << ", ";
            first = false;
            auto ids = tp->IDENTIFIER();
            if (!ids.empty())
                label << ids[0]->getText();
            if (tp->COLON() && ids.size() >= 2)
                label << ": " << ids[1]->getText();
        }
        label << ">";
    }

    label << "(";

    if (auto* pl = func->paramList()) {
        auto params = pl->param();
        for (size_t i = 0; i < params.size(); i++) {
            if (i > 0) label << ", ";
            std::string pType = typeSpecToString(params[i]->typeSpec());
            std::string pName = safeText(params[i]->IDENTIFIER());
            bool isVariadic = params[i]->SPREAD() != nullptr;

            std::string paramLabel;
            if (isVariadic)
                paramLabel = pType + " ..." + pName;
            else
                paramLabel = pType + " " + pName;

            label << paramLabel;
            sig.parameters.push_back({paramLabel});
        }
    }

    label << ") " << retType;
    sig.label = label.str();

    // Attach doc-comment if available.
    size_t declLine = func->getStart()->getLine() - 1; // 0-based
    auto* doc = findDocForLine(docs, declLine);
    if (doc) sig.documentation = formatDocComment(*doc);

    return sig;
}

SignatureInfo SignatureHelpProvider::buildFromExtern(
        LucisParser::ExternDeclContext* ext,
        const std::vector<DocComment>& docs) {
    SignatureInfo sig;

    std::string retType = typeSpecToString(ext->typeSpec());
    std::string name = safeText(ext->IDENTIFIER());
    bool isVariadic = ext->SPREAD() != nullptr;

    std::ostringstream label;
    label << "extern " << retType << " " << name << "(";

    if (auto* epl = ext->externParamList()) {
        auto params = epl->externParam();
        for (size_t i = 0; i < params.size(); i++) {
            if (i > 0) label << ", ";
            std::string pType = typeSpecToString(params[i]->typeSpec());
            std::string pName;
            if (params[i]->IDENTIFIER())
                pName = safeText(params[i]->IDENTIFIER());
            else
                pName = paramNameFromType(pType);

            std::string paramLabel = pType + " " + pName;
            label << paramLabel;
            sig.parameters.push_back({paramLabel});
        }
    }

    if (isVariadic) {
        if (!sig.parameters.empty()) label << ", ";
        label << "...";
    }

    label << ")";
    sig.label = label.str();

    size_t declLine = ext->getStart()->getLine() - 1;
    auto* doc = findDocForLine(docs, declLine);
    if (doc) sig.documentation = formatDocComment(*doc);

    return sig;
}

SignatureInfo SignatureHelpProvider::buildFromBuiltin(const BuiltinSignature& bs) {
    SignatureInfo sig;

    std::ostringstream label;
    label << bs.returnType << " " << bs.name << "(";

    for (size_t i = 0; i < bs.paramTypes.size(); i++) {
        if (i > 0) label << ", ";
        std::string pType = bs.paramTypes[i];
        std::string pName = paramNameFromType(pType);
        bool consumes = false;
        bool borrows = false;
        for (size_t idx : bs.consumingArgs) if (idx == i) consumes = true;
        for (size_t idx : bs.borrowedArgs) if (idx == i) borrows = true;

        std::string paramLabel = pType + " " + pName;
        if (consumes) paramLabel += " [consumes]";
        else if (borrows) paramLabel += " [borrow]";
        label << paramLabel;
        sig.parameters.push_back({paramLabel});
    }

    if (bs.isVariadic) {
        if (!bs.paramTypes.empty()) label << ", ";
        label << "...";
    }

    label << ")";
    sig.label = label.str();
    if (bs.returnsOwned) {
        sig.documentation = "Returns owned memory (caller/language autodrop owns this value).\n\n";
    }
    if (!bs.description.empty()) {
        sig.documentation += bs.description;
    }

    return sig;
}

SignatureInfo SignatureHelpProvider::buildFromIntrinsic(const IntrinsicFunction& intrinsic) {
    SignatureInfo sig;

    std::ostringstream label;
    label << intrinsic.returnType << " lucis::" << intrinsic.name << "(";

    for (size_t i = 0; i < intrinsic.params.size(); i++) {
        if (i > 0) label << ", ";
        std::string pType = intrinsic.params[i].type;
        std::string pName = paramNameFromType(pType);

        std::string paramLabel = pType + " " + pName;
        label << paramLabel;
        sig.parameters.push_back({paramLabel});
    }

    if (intrinsic.isVariadic) {
        if (!intrinsic.params.empty()) label << ", ";
        label << "...";
    }

    label << ")";
    sig.label = label.str();

    if (!intrinsic.description.empty()) {
        sig.documentation = intrinsic.description;
    }

    return sig;
}

SignatureInfo SignatureHelpProvider::buildFromCFunction(const CFunction& func) {
    SignatureInfo sig;

    std::string retType = func.returnType ? func.returnType->name : "void";

    std::ostringstream label;
    label << retType << " " << func.name << "(";

    for (size_t i = 0; i < func.paramTypes.size(); i++) {
        if (i > 0) label << ", ";
        std::string pType = func.paramTypes[i] ? func.paramTypes[i]->name : "void";
        // Prefer actual parameter name from header; fall back to type-inferred
        std::string pName = (i < func.paramNames.size() && !func.paramNames[i].empty())
                            ? func.paramNames[i]
                            : paramNameFromType(pType);

        std::string paramLabel = pType + " " + pName;
        label << paramLabel;
        sig.parameters.push_back({paramLabel});
    }

    if (func.isVariadic) {
        if (!func.paramTypes.empty()) label << ", ";
        label << "...";
    }

    label << ")";
    sig.label = label.str();

    if (!func.doc.empty())
        sig.documentation = func.doc;

    return sig;
}

SignatureInfo SignatureHelpProvider::buildFromExtendMethod(
        LucisParser::ExtendMethodContext* method,
        const std::string& structName,
        const std::vector<DocComment>& docs) {
    SignatureInfo sig;

    std::string retType = typeSpecToString(method->typeSpec());
    std::string name = safeIdAt(method, 0);
    bool hasReceiver = method->AMPERSAND() != nullptr;

    std::ostringstream label;
    label << "(" << structName << ") fn " << name << "(";

    // Collect params — skip the &self receiver parameter.
    std::vector<LucisParser::ParamContext*> params;
    if (hasReceiver) {
        params = method->param();
    } else {
        if (auto* pl = method->paramList())
            params = pl->param();
    }

    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) label << ", ";
        std::string pType = typeSpecToString(params[i]->typeSpec());
        std::string pName = safeText(params[i]->IDENTIFIER());
        bool isVariadic = params[i]->SPREAD() != nullptr;

        std::string paramLabel;
        if (isVariadic)
            paramLabel = pType + " ..." + pName;
        else
            paramLabel = pType + " " + pName;

        label << paramLabel;
        sig.parameters.push_back({paramLabel});
    }

    label << ") " << retType;
    sig.label = label.str();

    size_t declLine = method->getStart()->getLine() - 1;
    auto* doc = findDocForLine(docs, declLine);
    if (doc) sig.documentation = formatDocComment(*doc);

    return sig;
}

SignatureInfo SignatureHelpProvider::buildFromExtMethod(
        const MethodDescriptor& md,
        const std::string& receiverType) {
    SignatureInfo sig;

    // Resolve generic type parameters from the receiver.
    std::string baseName = receiverType;
    std::string elemType, keyType, valType;
    auto ltPos = receiverType.find('<');
    if (ltPos != std::string::npos) {
        baseName = receiverType.substr(0, ltPos);
        std::string inner = receiverType.substr(ltPos + 1);
        if (!inner.empty() && inner.back() == '>') inner.pop_back();

        // Split by comma for Map<K,V>
        auto commaPos = inner.find(',');
        if (commaPos != std::string::npos) {
            keyType = inner.substr(0, commaPos);
            valType = inner.substr(commaPos + 1);
            // Trim spaces
            while (!keyType.empty() && keyType.back() == ' ') keyType.pop_back();
            while (!valType.empty() && valType.front() == ' ') valType.erase(valType.begin());
        } else {
            elemType = inner;
        }
    }

    std::string retType = resolveTypePlaceholder(md.returnType, receiverType, elemType, keyType, valType);

    std::ostringstream label;
    label << "(" << receiverType << ") fn " << md.name << "(";

    for (size_t i = 0; i < md.paramTypes.size(); i++) {
        if (i > 0) label << ", ";
        std::string pType = resolveTypePlaceholder(md.paramTypes[i], receiverType, elemType, keyType, valType);
        std::string pName = paramNameFromType(pType);

        std::string paramLabel = pType + " " + pName;
        label << paramLabel;
        sig.parameters.push_back({paramLabel});
    }

    label << ") " << retType;
    sig.label = label.str();

    return sig;
}

// ═══════════════════════════════════════════════════════════════════════
//  Main entry point
// ═══════════════════════════════════════════════════════════════════════

// Forward declarations for static helpers used in signatureHelp.
static LucisParser::FunctionDeclContext*
findEnclosingFunction(LucisParser::ProgramContext* tree, size_t line);
static std::optional<std::vector<std::pair<std::string, std::string>>>
extractLambdaParams(LucisParser::ExpressionContext* expr);

std::optional<SignatureHelpResult>
SignatureHelpProvider::signatureHelp(
        const std::string& source, size_t line, size_t col,
        const std::string& filePath, const ProjectContext* project,
        ParseResult* preParsed) {
    // Analyze call site from raw text.
    auto site = analyzeCallSite(source, line, col);
    if (!site) return std::nullopt;

    // Parse the source for AST-based lookups.
    ParseResult localParseStorage;
    auto& parsed = preParsed ? *preParsed : (localParseStorage = Parser::parseString(source), localParseStorage);
    if (!parsed.tree) return std::nullopt;

    // Parse doc-comments.
    auto docs = parseDocComments(source);

    // Resolve C headers.
    CBindings localBindings;
    TypeRegistry cTypeReg;
    const CBindings* cBindingsPtr = nullptr;

    if (project && project->isValid()) {
        cBindingsPtr = &project->cBindings();
    } else {
        std::vector<LucisParser::IncludeDeclContext*> includes;
        for (auto* pre : parsed.tree->preambleDecl())
            if (auto* inc = pre->includeDecl()) includes.push_back(inc);
        if (!includes.empty()) {
            CHeaderResolver resolver(cTypeReg, localBindings);
            for (auto* incl : includes) {
                auto text = incl->getText();
                if (incl->INCLUDE_SYS()) {
                    auto header = CHeaderResolver::extractSystemHeader(text);
                    if (!header.empty())
                        resolver.resolveSystemHeader(header);
                } else if (incl->INCLUDE_LOCAL()) {
                    auto header = CHeaderResolver::extractLocalHeader(text);
                    if (!header.empty())
                        resolver.resolveLocalHeader(header, fs::path(filePath).parent_path().string());
                }
            }
        }
        cBindingsPtr = &localBindings;
    }

    const std::string& name = site->functionName;
    SignatureHelpResult result;

    // ── Method calls (obj.method(...)) ──────────────────────────────
    if (site->isMethodCall) {
        // 1) Extend methods in current file
        for (auto* tld : parsed.tree->topLevelDecl()) {
            auto* ext = tld->extendDecl();
            if (!ext || !ext->IDENTIFIER()) continue;
            std::string structName = safeText(ext->IDENTIFIER());
            for (auto* method : ext->extendMethod()) {
                if (method->IDENTIFIER().empty()) continue;
                if (safeIdAt(method, 0) == name) {
                    auto sig = buildFromExtendMethod(method, structName, docs);
                    sig.activeParameter = site->activeParam;
                    result.signatures.push_back(std::move(sig));
                }
            }
        }

        // 2) Extended type methods (Vec, Map, Set)
        // Try all extended types since we don't resolve the receiver statically.
        for (auto& baseName : extTypeRegistry_.allTypes()) {
            auto* desc = extTypeRegistry_.lookup(baseName);
            if (!desc) continue;
            for (auto& md : desc->methods) {
                if (md.name == name) {
                    // Build with a generic receiver for display.
                    std::string receiver;
                    if (desc->genericArity == 1) receiver = baseName + "<T>";
                    else if (desc->genericArity == 2) receiver = baseName + "<K, V>";
                    else receiver = baseName;

                    auto sig = buildFromExtMethod(md, receiver);
                    sig.activeParameter = site->activeParam;
                    result.signatures.push_back(std::move(sig));
                }
            }
        }

        // 3) MethodRegistry (primitive type methods)
        // Check all type kinds for a matching method.
        static const TypeKind allKinds[] = {
            TypeKind::Integer, TypeKind::Float,
            TypeKind::Bool, TypeKind::Char, TypeKind::String
        };
        bool foundPrimitive = false;
        for (auto kind : allKinds) {
            auto* md = methodRegistry_.lookup(kind, name);
            if (md && !foundPrimitive) {
                auto sig = buildFromExtMethod(*md, md->emitTag.substr(0, md->emitTag.find('.')));
                sig.activeParameter = site->activeParam;
                result.signatures.push_back(std::move(sig));
                foundPrimitive = true;
            }
        }

        // 3b) Array methods
        auto* arrMd = methodRegistry_.lookupArrayMethod(name);
        if (arrMd) {
            auto sig = buildFromExtMethod(*arrMd, "[]T");
            sig.activeParameter = site->activeParam;
            result.signatures.push_back(std::move(sig));
        }

        // 4) Cross-file extend methods
        if (project && project->isValid()) {
            for (auto& ns : project->registry().allModules()) {
                auto syms = project->registry().getModuleSymbols(ns);
                for (auto* sym : syms) {
                    if (sym->kind != ExportedSymbol::ExtendBlock) continue;
                    auto* ext = dynamic_cast<LucisParser::ExtendDeclContext*>(sym->decl);
                    if (!ext || !ext->IDENTIFIER()) continue;
                    std::string structName = safeText(ext->IDENTIFIER());
                    for (auto* method : ext->extendMethod()) {
                        if (method->IDENTIFIER().empty()) continue;
                        if (safeIdAt(method, 0) == name) {
                            auto sig = buildFromExtendMethod(method, structName, {});
                            sig.activeParameter = site->activeParam;
                            result.signatures.push_back(std::move(sig));
                        }
                    }
                }
            }
        }

        if (!result.signatures.empty()) {
            result.activeParameter = site->activeParam;
            return result;
        }
    }

    // ── Static method calls (Type::method(...)) ─────────────────────
    if (site->isStaticCall && !site->staticTypeName.empty()) {
        // 0) Intrinsic call: lucis::core::trap()
        if (intrinsicRegistry_.hasNamespace(site->staticTypeName)) {
            auto* intrinsic = intrinsicRegistry_.lookup(site->staticTypeName, name);
            if (intrinsic) {
                auto sig = buildFromIntrinsic(*intrinsic);
                sig.activeParameter = site->activeParam;
                result.signatures.push_back(std::move(sig));
                result.activeParameter = site->activeParam;
                return result;
            }
        }

        // 1) Extend blocks in current file
        for (auto* tld : parsed.tree->topLevelDecl()) {
            auto* ext = tld->extendDecl();
            if (!ext || !ext->IDENTIFIER()) continue;
            if (safeText(ext->IDENTIFIER()) != site->staticTypeName) continue;
            for (auto* method : ext->extendMethod()) {
                if (method->IDENTIFIER().empty()) continue;
                if (safeIdAt(method, 0) == name) {
                    auto sig = buildFromExtendMethod(method, site->staticTypeName, docs);
                    sig.activeParameter = site->activeParam;
                    result.signatures.push_back(std::move(sig));
                }
            }
        }

        // 2) Cross-file extend blocks
        if (result.signatures.empty() && project && project->isValid()) {
            for (auto& ns : project->registry().allModules()) {
                auto syms = project->registry().getModuleSymbols(ns);
                for (auto* sym : syms) {
                    if (sym->kind != ExportedSymbol::ExtendBlock) continue;
                    auto* ext = dynamic_cast<LucisParser::ExtendDeclContext*>(sym->decl);
                    if (!ext || !ext->IDENTIFIER()) continue;
                    if (safeText(ext->IDENTIFIER()) != site->staticTypeName) continue;
                    for (auto* method : ext->extendMethod()) {
                        if (method->IDENTIFIER().empty()) continue;
                        if (safeIdAt(method, 0) == name) {
                            // Read doc-comments from source file
                            std::vector<DocComment> crossDocs;
                            if (!sym->sourceFile.empty()) {
                                std::ifstream ifs(sym->sourceFile);
                                if (ifs.is_open()) {
                                    std::string crossSrc((std::istreambuf_iterator<char>(ifs)),
                                                          std::istreambuf_iterator<char>());
                                    crossDocs = parseDocComments(crossSrc);
                                }
                            }
                            auto sig = buildFromExtendMethod(method, site->staticTypeName, crossDocs);
                            sig.activeParameter = site->activeParam;
                            result.signatures.push_back(std::move(sig));
                        }
                    }
                }
            }
        }

        // 3) Extended type registry (Vec::new, Map::with_capacity, etc.)
        if (result.signatures.empty()) {
            auto* desc = extTypeRegistry_.lookup(site->staticTypeName);
            if (desc) {
                for (auto& md : desc->methods) {
                    if (md.name == name) {
                        auto sig = buildFromExtMethod(md, site->staticTypeName);
                        sig.activeParameter = site->activeParam;
                        result.signatures.push_back(std::move(sig));
                    }
                }
            }
        }

        if (!result.signatures.empty()) {
            result.activeParameter = site->activeParam;
            return result;
        }
    }

    // ── Free function calls ─────────────────────────────────────────

    // 1) User-defined function in current file
    for (auto* tld : parsed.tree->topLevelDecl()) {
        if (auto* func = tld->functionDecl()) {
            if (safeIdAt(func, 0) == name) {
                auto sig = buildFromFunction(func, docs);
                sig.activeParameter = site->activeParam;
                result.signatures.push_back(std::move(sig));
            }
        }
    }

    // 2) Extern declaration in current file
    for (auto* tld : parsed.tree->topLevelDecl()) {
        if (auto* ext = tld->externDecl()) {
            if (ext->IDENTIFIER() && safeText(ext->IDENTIFIER()) == name) {
                auto sig = buildFromExtern(ext, docs);
                sig.activeParameter = site->activeParam;
                result.signatures.push_back(std::move(sig));
            }
        }
    }

    // 3) Cross-file functions (imported via `use`)
    if (project && project->isValid()) {
        for (auto& ns : project->registry().allModules()) {
            auto* sym = project->registry().findSymbol(ns, name);
            if (!sym) continue;
            if (sym->kind == ExportedSymbol::Function) {
                auto* fd = dynamic_cast<LucisParser::FunctionDeclContext*>(sym->decl);
                if (fd) {
                    auto sig = buildFromFunction(fd, {});
                    sig.activeParameter = site->activeParam;
                    result.signatures.push_back(std::move(sig));
                }
            }
        }
    }

    // 4) Builtin function (only if no user/cross-file match)
    if (result.signatures.empty()) {
        auto* builtin = builtinRegistry_.lookup(name);
        if (builtin) {
            auto sig = buildFromBuiltin(*builtin);
            sig.activeParameter = site->activeParam;
            result.signatures.push_back(std::move(sig));
        }
    }

    // 5) C function (only if no match yet)
    if (result.signatures.empty()) {
        auto* cfunc = cBindingsPtr->findFunction(name);
        if (cfunc) {
            auto sig = buildFromCFunction(*cfunc);
            sig.activeParameter = site->activeParam;
            result.signatures.push_back(std::move(sig));
        }
    }

    // 5.5) C function-like macro
    if (result.signatures.empty()) {
        auto* cflm = cBindingsPtr->findFunctionLikeMacro(name);
        if (cflm) {
            SignatureInfo sig;
            sig.label = name;
            sig.activeParameter = site->activeParam;
            std::string paramsStr;
            for (size_t i = 0; i < cflm->paramNames.size(); i++) {
                if (i > 0) paramsStr += ", ";
                paramsStr += cflm->paramNames[i];
                ParameterInfo pi;
                pi.label = cflm->paramNames[i];
                sig.parameters.push_back(std::move(pi));
            }
            sig.label += "(" + paramsStr + ")";
            sig.documentation = "C function-like macro";
            result.signatures.push_back(std::move(sig));
        }
    }

    // 6) Fallback: check if name is a local variable initialized with a lambda
    if (result.signatures.empty()) {
        auto* func = findEnclosingFunction(parsed.tree, line);
        if (func && func->block()) {
            for (auto* stmt : func->block()->statement()) {
                auto* vd = stmt->varDeclStmt();
                if (!vd) continue;
                for (auto* d : vd->varDeclarator()) {
                    if (safeText(d->IDENTIFIER()) != name) continue;
                    auto* init = d->expression();
                    if (!init) break;
                    // Check for lambda expression: |params| expr or |params| { ... }
                    auto params = extractLambdaParams(init);
                    if (params) {
                        auto sig = buildFromParams(*params, name);
                        sig.activeParameter = site->activeParam;
                        result.signatures.push_back(std::move(sig));
                    }
                    break;
                }
            }
        }
    }

    if (result.signatures.empty()) return std::nullopt;

    result.activeParameter = site->activeParam;
    return result;
}

// Find enclosing function for a given line (0-based).
static LucisParser::FunctionDeclContext*
findEnclosingFunction(LucisParser::ProgramContext* tree, size_t line) {
    size_t tokenLine = line + 1;
    for (auto* tld : tree->topLevelDecl()) {
        auto* func = tld->functionDecl();
        if (!func) continue;
        auto* startTok = func->getStart();
        auto* stopTok  = func->getStop();
        if (!startTok || !stopTok) continue;
        if (tokenLine >= startTok->getLine() && tokenLine <= stopTok->getLine())
            return func;
    }
    return nullptr;
}

// Extract params from an expression if it is a lambda.
static std::optional<std::vector<std::pair<std::string, std::string>>>
extractLambdaParams(LucisParser::ExpressionContext* expr) {
    if (auto* le = dynamic_cast<LucisParser::LambdaExprContext*>(expr)) {
        auto* pl = le->paramList();
        if (!pl) return std::nullopt;
        std::vector<std::pair<std::string, std::string>> params;
        for (auto* p : pl->param()) {
            params.emplace_back(safeText(p->typeSpec()), safeText(p->IDENTIFIER()));
        }
        return params;
    }
    if (auto* lbe = dynamic_cast<LucisParser::LambdaBlockExprContext*>(expr)) {
        auto* pl = lbe->paramList();
        if (!pl) return std::nullopt;
        std::vector<std::pair<std::string, std::string>> params;
        for (auto* p : pl->param()) {
            params.emplace_back(safeText(p->typeSpec()), safeText(p->IDENTIFIER()));
        }
        return params;
    }
    return std::nullopt;
}

SignatureInfo
SignatureHelpProvider::buildFromParams(const std::vector<LambdaParam>& params,
                                       const std::string& funcName) {
    SignatureInfo sig;
    std::ostringstream label;
    label << funcName << "(";
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) label << ", ";
        std::string paramLabel = params[i].first;
        if (!params[i].second.empty())
            paramLabel += " " + params[i].second;
        label << paramLabel;
        sig.parameters.push_back({paramLabel});
    }
    label << ") ?";
    sig.label = label.str();
    return sig;
}
