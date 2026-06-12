#include "lsp/HoverProvider.h"
#include "lsp/ProjectContext.h"
#include "lsp/DocComment.h"
#include "parser/Parser.h"
#include "ffi/CHeaderResolver.h"
#include "namespace/NamespaceRegistry.h"
#include "imports/ImportResolver.h"

#include <sstream>
#include <fstream>
#include <filesystem>

HoverProvider::HoverProvider() : intrinsicRegistry_(typeRegistry_) {}

// Normalize lowercase native keywords (vec, map, set) to registry CamelCase (Vec, Map, Set)
static std::string normalizeExtBaseName(const std::string& name) {
    if (name == "vec") return "Vec";
    if (name == "map") return "Map";
    if (name == "set") return "Set";
    return name;
}

// Context for resolving function/method call return types during auto inference.
struct FuncLookupCtx {
    LuxParser::ProgramContext* tree = nullptr;
    const CBindings* bindings = nullptr;
    const BuiltinRegistry* builtinReg = nullptr;
    const IntrinsicRegistry* intrinsicReg = nullptr;
    const ExtendedTypeRegistry* extTypeReg = nullptr;
    const MethodRegistry* methodReg = nullptr;
    const ProjectContext* project = nullptr;
};

// Forward declarations
static std::string inferExprTypeName(
        LuxParser::ExpressionContext* expr,
        const std::unordered_map<std::string, HoverProvider::LocalVar>& locals,
        const FuncLookupCtx* flc);

static std::string trimCopy(const std::string& s);
static std::vector<std::string> splitTopLevelComma(const std::string& s);
static bool parseGenericInstance(const std::string& t,
                                 std::string& base,
                                 std::vector<std::string>& args);
static std::string substituteTypeParams(
    const std::string& type,
    const std::unordered_map<std::string, std::string>& subst);
static bool unifyGenericType(
    const std::string& formalRaw,
    const std::string& actualRaw,
    const std::unordered_set<std::string>& typeParams,
    std::unordered_map<std::string, std::string>& inferred);

static std::string safeText(antlr4::tree::TerminalNode *n) {
    return n ? n->getText() : "";
}
static std::string safeText(antlr4::ParserRuleContext *ctx) {
    return ctx ? ctx->getText() : "";
}

static std::string extractBaseTypeName(LuxParser::TypeSpecContext* typeSpec) {
    auto text = typeSpec->getText();
    auto pos = text.find('<');
    if (pos != std::string::npos)
        text.resize(pos);
    while (!text.empty() && text.back() == ' ') text.pop_back();
    return text;
}

template<typename T>
static std::string safeIdAt(T *ctx, size_t i) {
    if (!ctx) return "";
    if (i >= ctx->IDENTIFIER().size()) return "";
    auto *n = ctx->IDENTIFIER(i);
    return n ? n->getText() : "";
}

static bool isCHoverType(const std::string& rawType, const CBindings& bindings) {
    std::string t = rawType;

    auto trim = [](std::string s) {
        auto b = s.find_first_not_of(" \t\n\r");
        if (b == std::string::npos) return std::string{};
        auto e = s.find_last_not_of(" \t\n\r");
        return s.substr(b, e - b + 1);
    };

    t = trim(t);
    while (t.rfind("[]", 0) == 0)
        t = t.substr(2);
    while (!t.empty() && t.front() == '*')
        t.erase(t.begin());
    t = trim(t);

    auto lt = t.find('<');
    if (lt != std::string::npos)
        t = trim(t.substr(0, lt));

    if (t.empty()) return false;
    return bindings.findTypedef(t) || bindings.findStruct(t) || bindings.findEnum(t);
}

static std::string formatTypedHover(const std::string& kind,
                                    const std::string& typeName,
                                    const std::string& symbolName,
                                    const CBindings& bindings) {
    const char* lang = isCHoverType(typeName, bindings) ? "c" : "lux";
    return std::string("```") + lang + "\n(" + kind + ") " + typeName + " "
         + symbolName + "\n```";
}

static std::string normalizeHoverMarkdown(std::string contents) {
    // Normalize to the language id recognized by markdown hover rendering.
    const std::string from = "```lx";
    const std::string to = "```lux";
    size_t pos = 0;
    while ((pos = contents.find(from, pos)) != std::string::npos) {
        contents.replace(pos, from.size(), to);
        pos += to.size();
    }

    // Some paths still produce plain typed text like "(field) ...".
    // Wrap those in a Lux fenced block so hover has syntax highlighting.
    if (contents.find("```") == std::string::npos) {
        size_t first = contents.find_first_not_of(" \t\n\r");
        if (first != std::string::npos && contents[first] == '(') {
            std::string raw = contents.substr(first);
            if (!raw.empty() && raw.back() == '\n') raw.pop_back();
            contents = "```lux\n" + raw + "\n```";
        }
    }

    return contents;
}

// ═══════════════════════════════════════════════════════════════════════
//  Doc-comment helper
// ═══════════════════════════════════════════════════════════════════════

std::string HoverProvider::withDoc(const std::string& md, size_t declLine) {
    return appendDocToHover(md, docComments_, declLine);
}

const std::vector<DocComment>&
HoverProvider::docCommentsForFile(const std::string& filePath) {
    static const std::vector<DocComment> kEmpty;
    if (filePath.empty()) return kEmpty;

    std::error_code ec;
    auto mtime = std::filesystem::last_write_time(filePath, ec);
    bool hasMtime = !ec;

    auto it = crossFileDocCache_.find(filePath);
    if (it != crossFileDocCache_.end()) {
        bool stale = false;
        if (hasMtime) {
            stale = (!it->second.hasMtime) || (it->second.mtime != mtime);
        }
        if (!stale) return it->second.docs;
    }

    DocCommentCacheEntry entry;
    entry.hasMtime = hasMtime;
    if (hasMtime) entry.mtime = mtime;

    std::ifstream f(filePath);
    if (f) {
        std::string source((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
        entry.docs = parseDocComments(source);
    }

    auto [newIt, inserted] = crossFileDocCache_.insert_or_assign(filePath, std::move(entry));
    (void)inserted;
    return newIt->second.docs;
}

static void collectLocalsFromBlock(
        LuxParser::BlockContext* block,
        size_t beforeLine,
        std::unordered_map<std::string, HoverProvider::LocalVar>& out,
        const FuncLookupCtx* flc = nullptr);

// ═══════════════════════════════════════════════════════════════════════
//  Main entry point (single-file mode)
// ═══════════════════════════════════════════════════════════════════════

std::optional<HoverResult> HoverProvider::hover(const std::string& source,
                                                 size_t line, size_t col,
                                                 ParseResult* preParsed) {
    return hover(source, line, col, "", nullptr, preParsed);
}

// ═══════════════════════════════════════════════════════════════════════
//  Main entry point (project-aware mode)
// ═══════════════════════════════════════════════════════════════════════

std::optional<HoverResult> HoverProvider::hover(const std::string& source,
                                                 size_t line, size_t col,
                                                 const std::string& filePath,
                                                 const ProjectContext* project,
                                                 ParseResult* preParsed) {
    ParseResult localParseStorage;
    auto& parsed = preParsed ? *preParsed : (localParseStorage = Parser::parseString(source), localParseStorage);
    if (!parsed.tree) return std::nullopt;

    // Parse doc-comments from raw source for hover enrichment.
    docComments_ = parseDocComments(source);

    // Resolve C headers — use project-level bindings if available.
    CBindings localBindings;
    TypeRegistry cTypeReg;
    const CBindings* cBindingsPtr = nullptr;

    if (project && project->isValid()) {
        cBindingsPtr = &project->cBindings();
    } else {
        std::vector<LuxParser::IncludeDeclContext*> includes;
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
                        resolver.resolveLocalHeader(header, ".");
                }
            }
        }
        cBindingsPtr = &localBindings;
    }

    // Find the token at the cursor (LSP 0-based → ANTLR 1-based line).
    size_t tokenLine = line + 1;
    antlr4::Token* hoveredToken = nullptr;

    for (auto& token : parsed.tokens->getTokens()) {
        if (token->getType() == antlr4::Token::EOF) continue;
        if (token->getLine() == tokenLine) {
            size_t start = token->getCharPositionInLine();
            size_t end   = start + token->getText().length();
            if (col >= start && col < end) {
                hoveredToken = token;
                break;
            }
        }
    }
    if (!hoveredToken) return std::nullopt;

    // Skip non-identifier tokens (operators, punctuation, keywords, etc.)
    // We do handle some keyword tokens for type names below.
    auto tokenType = hoveredToken->getType();
    std::string tokenText = hoveredToken->getText();

    // Walk the parse tree to find the context of this token.
    // Strategy: iterate top-level declarations and expressions to find a match.

    // ── Namespace declaration: namespace Main; ──────────────────────
    if (auto* nsDecl = parsed.tree->namespaceDecl()) {
        if (nsDecl->IDENTIFIER() && nsDecl->IDENTIFIER()->getSymbol() == hoveredToken) {
            std::string md = "```lux\n(namespace) " + tokenText + "\n```";
            return makeResult(hoveredToken, md);
        }
    }

    // ── Use declarations: use Utils::{ add, Role, User }; ──────────
    for (auto* pre : parsed.tree->preambleDecl()) {
        auto* useDecl = pre->useDecl();
        if (!useDecl) continue;
        if (auto* root = dynamic_cast<LuxParser::UseRootContext*>(useDecl)) {
            if (root->IDENTIFIER() && root->IDENTIFIER()->getSymbol() == hoveredToken) {
                std::string md = "```lux\n(module) " + safeText(root->IDENTIFIER()) + "\n```";
                return makeResult(hoveredToken, md);
            }
        }
        if (auto* item = dynamic_cast<LuxParser::UseItemContext*>(useDecl)) {
            if (!item->IDENTIFIER() || !item->modulePath()) continue;
            // Hover on the imported symbol name
            if (item->IDENTIFIER()->getSymbol() == hoveredToken) {
                auto symbolName = safeText(item->IDENTIFIER());
                // Resolve via project context
                std::string modulePath;
                for (auto* id : item->modulePath()->IDENTIFIER()) {
                    if (!modulePath.empty()) modulePath += "::";
                    modulePath += id->getText();
                }
                return hoverImportedSymbol(symbolName, modulePath,
                    hoveredToken, parsed.tree, *cBindingsPtr, project);
            }
            // Hover on module path identifiers
            for (auto* id : item->modulePath()->IDENTIFIER()) {
                if (id->getSymbol() == hoveredToken) {
                    std::string md = "```lux\n(module) " + item->modulePath()->getText() + "\n```";
                    return makeResult(hoveredToken, md);
                }
            }
        }
        if (auto* group = dynamic_cast<LuxParser::UseGroupContext*>(useDecl)) {
            if (!group->modulePath()) continue;
            std::string modulePath;
            for (auto* id : group->modulePath()->IDENTIFIER()) {
                if (!modulePath.empty()) modulePath += "::";
                modulePath += id->getText();
            }
            // Hover on module path identifiers
            for (auto* id : group->modulePath()->IDENTIFIER()) {
                if (id->getSymbol() == hoveredToken) {
                    std::string md = "```lux\n(module) " + modulePath + "\n```";
                    return makeResult(hoveredToken, md);
                }
            }
            // Hover on imported symbol names
            for (auto* id : group->IDENTIFIER()) {
                if (id->getSymbol() == hoveredToken) {
                    return hoverImportedSymbol(id->getText(), modulePath,
                        hoveredToken, parsed.tree, *cBindingsPtr, project);
                }
            }
        }
    }

    // ── Check if hovering a function name in a functionDecl ─────────
    for (auto* tld : parsed.tree->topLevelDecl()) {
        if (auto* func = tld->functionDecl()) {
            if (func->IDENTIFIER().empty()) continue;
            auto* nameToken = func->IDENTIFIER(0)->getSymbol();
            if (nameToken == hoveredToken) {
                return makeResult(nameToken, withDoc(formatFunctionDecl(func), func->getStart()->getLine() - 1));
            }
            // Check parameter names
            if (auto* params = func->paramList()) {
                for (auto* p : params->param()) {
                    if (!p->IDENTIFIER()) continue;
                    auto* pName = p->IDENTIFIER()->getSymbol();
                    if (pName == hoveredToken) {
                        std::string typeStr = typeSpecToString(p->typeSpec());
                        std::string md = formatTypedHover("parameter", typeStr,
                                                          tokenText, *cBindingsPtr);
                        return makeResult(hoveredToken, md);
                    }
                    // Hover on param type
                    // handled by the type-spec walk below
                }
            }
        }

        // ── Struct declaration name ─────────────────────────────────
        if (auto* sd = tld->structDecl()) {
            if (!sd->IDENTIFIER()) continue;
            if (sd->IDENTIFIER()->getSymbol() == hoveredToken) {
                return makeResult(hoveredToken, withDoc(formatStructDecl(sd), sd->getStart()->getLine() - 1));
            }
            // Hover on field names inside struct
            for (auto* f : sd->structField()) {
                if (!f->IDENTIFIER()) continue;
                if (f->IDENTIFIER()->getSymbol() == hoveredToken) {
                    std::string md = "```lux\n(field) " + typeSpecToString(f->typeSpec())
                                   + " " + tokenText + "\n```";
                    return makeResult(hoveredToken, md);
                }
            }
        }

        // ── Enum declaration name ───────────────────────────────────
        if (auto* ed = tld->enumDecl()) {
            auto* enumName = ed->IDENTIFIER();
            if (!enumName) continue;
            if (enumName->getSymbol() == hoveredToken) {
                return makeResult(hoveredToken, withDoc(formatEnumDecl(ed), ed->getStart()->getLine() - 1));
            }
            // Hover on variant name
            for (auto* variant : ed->enumVariant()) {
                auto* v = variant->IDENTIFIER();
                if (v && v->getSymbol() == hoveredToken) {
                    std::string md = "```lux\n(variant) " + enumName->getText()
                                   + "::" + v->getText() + "\n```";
                    return makeResult(hoveredToken, md);
                }
            }
        }

        // ── Union declaration name ──────────────────────────────────
        if (auto* ud = tld->unionDecl()) {
            if (!ud->IDENTIFIER()) continue;
            if (ud->IDENTIFIER()->getSymbol() == hoveredToken) {
                return makeResult(hoveredToken, withDoc(formatUnionDecl(ud), ud->getStart()->getLine() - 1));
            }
            for (auto* f : ud->unionField()) {
                if (!f->IDENTIFIER()) continue;
                if (f->IDENTIFIER()->getSymbol() == hoveredToken) {
                    std::string md = "```lux\n(field) " + typeSpecToString(f->typeSpec())
                                   + " " + tokenText + "\n```";
                    return makeResult(hoveredToken, md);
                }
            }
        }

        // ── Extend declaration name ─────────────────────────────────
        if (auto* ext = tld->extendDecl()) {
            if (!ext->IDENTIFIER()) continue;
            if (ext->IDENTIFIER()->getSymbol() == hoveredToken) {
                std::string md = withDoc(formatExtendMethods(ext), ext->getStart()->getLine() - 1);
                return makeResult(hoveredToken, md);
            }
        }

        // ── Type alias declaration ──────────────────────────────────
        if (auto* ta = tld->typeAliasDecl()) {
            if (!ta->IDENTIFIER()) continue;
            if (ta->IDENTIFIER()->getSymbol() == hoveredToken) {
                std::string md = "```lux\ntype " + tokenText + " = "
                               + typeSpecToString(ta->typeSpec()) + "\n```";
                return makeResult(hoveredToken, withDoc(md, ta->getStart()->getLine() - 1));
            }
        }

        // ── Extern declaration name ─────────────────────────────────
        if (auto* ext = tld->externDecl()) {
            if (!ext->IDENTIFIER()) continue;
            if (ext->IDENTIFIER()->getSymbol() == hoveredToken) {
                return makeResult(hoveredToken, withDoc(formatExternDecl(ext), ext->getStart()->getLine() - 1));
            }
        }
    }

    // ── Resolve identifier expressions inside function bodies ───────
    // Walk into function bodies to find expressions containing the hovered token.
    // This handles method calls, field access, enum access, static method calls,
    // type specs in varDecl/params, and plain identifiers.

    for (auto* tld : parsed.tree->topLevelDecl()) {
        auto* func = tld->functionDecl();
        if (func) {
            auto* startTok = func->getStart();
            auto* stopTok  = func->getStop();
            if (startTok && stopTok &&
                tokenLine >= startTok->getLine() && tokenLine <= stopTok->getLine()) {
                auto result = walkTreeForHover(func->block(), hoveredToken, tokenText,
                                               parsed.tree, *cBindingsPtr, line, project);
                if (result) return result;
            }
            continue;
        }

        // Walk into extend block method bodies
        auto* ext = tld->extendDecl();
        if (ext) {
            auto* startTok = ext->getStart();
            auto* stopTok  = ext->getStop();
            if (!startTok || !stopTok) continue;
            if (tokenLine < startTok->getLine() || tokenLine > stopTok->getLine())
                continue;

            // Check extend method parameter names and type specs
            for (auto* method : ext->extendMethod()) {
                auto* mStart = method->getStart();
                auto* mStop  = method->getStop();
                if (!mStart || !mStop) continue;
                if (tokenLine < mStart->getLine() || tokenLine > mStop->getLine())
                    continue;

                // Method name
                if (method->IDENTIFIER(0) && method->IDENTIFIER(0)->getSymbol() == hoveredToken) {
                    std::string structName = ext->IDENTIFIER() ? safeText(ext->IDENTIFIER()) : "?";
                    bool isStatic = (method->AMPERSAND() == nullptr);
                    std::string retType = typeSpecToString(method->typeSpec());
                    std::string md = "```lux\n(" + std::string(isStatic ? "static method" : "method")
                        + ") " + retType + " " + structName + "::" + tokenText + "(";
                    std::vector<LuxParser::ParamContext*> params;
                    if (isStatic) {
                        if (auto* pl = method->paramList())
                            params = pl->param();
                    } else {
                        params = method->param();
                    }
                    bool first = true;
                    if (!isStatic) { md += "&self"; first = false; }
                    for (auto* p : params) {
                        if (!first) md += ", ";
                        first = false;
                        md += typeSpecToString(p->typeSpec()) + " "
                            + (p->IDENTIFIER() ? safeText(p->IDENTIFIER()) : "?");
                    }
                    md += ")\n```";
                    return makeResult(hoveredToken, md);
                }

                // Return type spec
                auto r = hoverTypeSpec(method->typeSpec(), hoveredToken,
                                        parsed.tree, *cBindingsPtr, project);
                if (r) return r;

                // Parameter names and types
                bool isStatic = (method->AMPERSAND() == nullptr);
                std::vector<LuxParser::ParamContext*> params;
                if (isStatic) {
                    if (auto* pl = method->paramList())
                        params = pl->param();
                } else {
                    params = method->param();
                }
                for (auto* p : params) {
                    if (!p->IDENTIFIER()) continue;
                    if (p->IDENTIFIER()->getSymbol() == hoveredToken) {
                        std::string md = formatTypedHover(
                            "parameter",
                            typeSpecToString(p->typeSpec()),
                            safeText(p->IDENTIFIER()),
                            *cBindingsPtr);
                        return makeResult(hoveredToken, md);
                    }
                    r = hoverTypeSpec(p->typeSpec(), hoveredToken,
                                       parsed.tree, *cBindingsPtr, project);
                    if (r) return r;
                }

                // Walk method body
                auto result = walkTreeForHover(method->block(), hoveredToken,
                    tokenText, parsed.tree, *cBindingsPtr, line, project);
                if (result) return result;
            }
        }
    }

    // ── Fallback: resolve any IDENTIFIER (global scope or missed contexts) ──
    if (tokenType == LuxLexer::IDENTIFIER) {
        return hoverIdent(tokenText, hoveredToken, parsed.tree,
                          *cBindingsPtr, line, project);
    }

    // ── Primitive type keywords (int32, float64, bool, etc.) ────────
    // These are their own token types, not IDENTIFIER.
    const TypeInfo* primitiveType = typeRegistry_.lookup(tokenText);
    if (primitiveType) {
        std::string md = "```lux\n(type) " + primitiveType->name + "\n```";
        return makeResult(hoveredToken, md);
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  Hover on identifier
// ═══════════════════════════════════════════════════════════════════════

std::optional<HoverResult> HoverProvider::hoverIdent(
        const std::string& name, antlr4::Token* token,
        LuxParser::ProgramContext* tree, const CBindings& bindings,
        size_t cursorLine, const ProjectContext* project) {

    // 1) Local variable / parameter in enclosing function
    auto* func = findEnclosingFunction(tree, cursorLine);
    if (func) {
        auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
        auto it = locals.find(name);
        if (it != locals.end()) {
            std::string dims;
            for (unsigned i = 0; i < it->second.arrayDims; i++)
                dims += "[]";
            std::string kind = it->second.isParameter ? "parameter" : "variable";
            std::string md = formatTypedHover(kind,
                                              dims + it->second.typeName,
                                              name,
                                              bindings);
            return makeResult(token, md);
        }
    }

    // 1.5) `self` keyword inside extend block method
    if (name == "self") {
        size_t tokenLine = cursorLine + 1; // to 1-based
        for (auto* tld : tree->topLevelDecl()) {
            auto* ext = tld->extendDecl();
            if (!ext) continue;
            auto* es = ext->getStart();
            auto* ee = ext->getStop();
            if (!es || !ee) continue;
            if (tokenLine < es->getLine() || tokenLine > ee->getLine()) continue;
            if (!ext->IDENTIFIER()) continue;
            std::string structName = safeText(ext->IDENTIFIER());
            std::string md = "```lux\n(self) *" + structName + "\n```";
            return makeResult(token, md);
        }
    }

    // 1.6) Parameter or local inside extend method body
    if (!func) {
        size_t tokenLine = cursorLine + 1;
        for (auto* tld : tree->topLevelDecl()) {
            auto* ext = tld->extendDecl();
            if (!ext) continue;
            for (auto* method : ext->extendMethod()) {
                auto* ms = method->getStart();
                auto* me = method->getStop();
                if (!ms || !me) continue;
                if (tokenLine < ms->getLine() || tokenLine > me->getLine()) continue;
                // Check parameters
                bool isStatic = (method->AMPERSAND() == nullptr);
                std::vector<LuxParser::ParamContext*> params;
                if (isStatic) {
                    if (auto* pl = method->paramList())
                        params = pl->param();
                } else {
                    params = method->param();
                }
                for (auto* p : params) {
                    if (!p->IDENTIFIER()) continue;
                    if (safeText(p->IDENTIFIER()) == name) {
                        std::string md = formatTypedHover(
                            "parameter",
                            typeSpecToString(p->typeSpec()),
                            name,
                            bindings);
                        return makeResult(token, md);
                    }
                }
                // Collect locals from method body
                std::unordered_map<std::string, LocalVar> locals;
                collectLocalsFromBlock(method->block(), tokenLine, locals);
                auto it = locals.find(name);
                if (it != locals.end()) {
                    std::string dims;
                    for (unsigned i = 0; i < it->second.arrayDims; i++)
                        dims += "[]";
                    std::string md = formatTypedHover("variable",
                                                      dims + it->second.typeName,
                                                      name,
                                                      bindings);
                    return makeResult(token, md);
                }
            }
        }
    }

    // 2) User-defined function
    auto* funcDecl = findFunctionDecl(tree, name);
    if (funcDecl) {
        return makeResult(token, withDoc(formatFunctionDecl(funcDecl), funcDecl->getStart()->getLine() - 1));
    }

    // 2b) Extern function declaration
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* ext = tld->externDecl()) {
            if (ext->IDENTIFIER() && safeText(ext->IDENTIFIER()) == name) {
                return makeResult(token, withDoc(formatExternDecl(ext), ext->getStart()->getLine() - 1));
            }
        }
    }

    // 3) Cross-file symbol from project registry (imported via `use`)
    if (project && project->isValid()) {
        for (auto& ns : project->registry().allNamespaces()) {
            auto* sym = project->registry().findSymbol(ns, name);
            if (!sym) continue;

            // Read source file and parse doc-comments for the cross-file symbol.
            const auto& crossDocs = docCommentsForFile(sym->sourceFile);

            switch (sym->kind) {
                case ExportedSymbol::Function:
                    if (auto* fd = dynamic_cast<LuxParser::FunctionDeclContext*>(sym->decl)) {
                        std::string crossMd = formatFunctionDecl(fd);
                        size_t declLine = fd->getStart()->getLine() - 1;
                        crossMd = appendDocToHover(crossMd, crossDocs, declLine);
                        return makeResult(token, crossMd);
                    }
                    break;
                case ExportedSymbol::Struct:
                    if (auto* sd = dynamic_cast<LuxParser::StructDeclContext*>(sym->decl)) {
                        std::string crossMd = formatStructDecl(sd);
                        size_t declLine = sd->getStart()->getLine() - 1;
                        crossMd = appendDocToHover(crossMd, crossDocs, declLine);
                        return makeResult(token, crossMd);
                    }
                    break;
                case ExportedSymbol::Enum:
                    if (auto* ed = dynamic_cast<LuxParser::EnumDeclContext*>(sym->decl)) {
                        std::string crossMd = formatEnumDecl(ed);
                        size_t declLine = ed->getStart()->getLine() - 1;
                        crossMd = appendDocToHover(crossMd, crossDocs, declLine);
                        return makeResult(token, crossMd);
                    }
                    break;
                case ExportedSymbol::Union:
                    if (auto* ud = dynamic_cast<LuxParser::UnionDeclContext*>(sym->decl)) {
                        std::string crossMd = formatUnionDecl(ud);
                        size_t declLine = ud->getStart()->getLine() - 1;
                        crossMd = appendDocToHover(crossMd, crossDocs, declLine);
                        return makeResult(token, crossMd);
                    }
                    break;
                case ExportedSymbol::TypeAlias:
                    if (auto* ta = dynamic_cast<LuxParser::TypeAliasDeclContext*>(sym->decl)) {
                        std::string md = "```lux\ntype " + name + " = "
                                       + typeSpecToString(ta->typeSpec()) + "\n```";
                        size_t declLine = ta->getStart()->getLine() - 1;
                        md = appendDocToHover(md, crossDocs, declLine);
                        return makeResult(token, md);
                    }
                    break;
                case ExportedSymbol::ExtendBlock:
                    if (auto* ext = dynamic_cast<LuxParser::ExtendDeclContext*>(sym->decl))
                        return makeResult(token, formatExtendMethods(ext));
                    break;
            }
        }
    }

    // 4) Builtin function
    auto* builtin = builtinRegistry_.lookup(name);
    if (builtin) {
        return makeResult(token, formatBuiltinSignature(*builtin));
    }

    // 5) Builtin constant (PI, E, INT32_MAX, etc.)
    auto& constType = builtinRegistry_.lookupConstant(name);
    if (!constType.empty()) {
        std::string md = "```lux\n(constant) " + constType + " " + name + "\n```";
        return makeResult(token, md);
    }

    // 6) C function
    auto* cfunc = bindings.findFunction(name);
    if (cfunc) {
        return makeResult(token, formatCFunction(*cfunc));
    }

    // 7) C struct (used as type or in struct literal)
    auto* cstruct = bindings.findStruct(name);
    if (cstruct) {
        return makeResult(token, formatCStruct(name, *cstruct));
    }

    // 7) C enum
    auto* cenum = bindings.findEnum(name);
    if (cenum) {
        return makeResult(token, formatCEnum(name, *cenum));
    }

    // 8) C macro constant
    auto* cmacro = bindings.findMacro(name);
    if (cmacro) {
        std::string valStr;
        if (cmacro->isFloat)
            valStr = std::to_string(cmacro->floatValue);
        else if (cmacro->isString)
            valStr = "\"" + cmacro->stringValue + "\"";
        else
            valStr = std::to_string(cmacro->value);
        std::string md = "```c\n#define " + name + " " + valStr + "\n```";
        return makeResult(token, md);
    }

    // 9) C struct macro (e.g. RAYWHITE → Color{245,245,245,255})
    auto* csmacro = bindings.findStructMacro(name);
    if (csmacro) {
        std::string md = "```c\n#define " + name + " " + csmacro->structType + "{ ";
        for (size_t i = 0; i < csmacro->fieldValues.size(); i++) {
            if (i > 0) md += ", ";
            md += std::to_string(csmacro->fieldValues[i]);
        }
        md += " }\n```";
        return makeResult(token, md);
    }

    // 9.5) C enum variant value (e.g. FLAG_VSYNC_HINT from ConfigFlags)
    for (auto& [enumName, cenum] : bindings.enums()) {
        for (auto& [vname, val] : cenum.values) {
            if (vname == name) {
                std::string md = "```c\n(enum value) " + enumName + "::" + name
                               + " = " + std::to_string(val) + "\n```\n*C enum variant*";
                return makeResult(token, md);
            }
        }
    }

    // 10) C global variable
    auto* cglobal = bindings.findGlobal(name);
    if (cglobal) {
        std::string md = "```c\n(global) " + cglobal->type->name + " " + name + "\n```";
        return makeResult(token, md);
    }

    // 11) C typedef
    auto* ctdef = bindings.findTypedef(name);
    if (ctdef) {
        std::string md = "```c\ntypedef " + ctdef->underlying->name + " " + name + "\n```";
        return makeResult(token, md);
    }

    // 12) User-defined struct type
    auto* structDecl = findStructDecl(tree, name);
    if (structDecl) {
        return makeResult(token, withDoc(formatStructDecl(structDecl), structDecl->getStart()->getLine() - 1));
    }

    // 13) User-defined enum type
    auto* enumDecl = findEnumDecl(tree, name);
    if (enumDecl) {
        return makeResult(token, withDoc(formatEnumDecl(enumDecl), enumDecl->getStart()->getLine() - 1));
    }

    // 14) User-defined union type
    auto* unionDecl = findUnionDecl(tree, name);
    if (unionDecl) {
        return makeResult(token, withDoc(formatUnionDecl(unionDecl), unionDecl->getStart()->getLine() - 1));
    }

    // 15) Type alias
    auto* aliasDecl = findTypeAliasDecl(tree, name);
    if (aliasDecl) {
        std::string md = "```lux\ntype " + name + " = "
                       + typeSpecToString(aliasDecl->typeSpec()) + "\n```";
        return makeResult(token, withDoc(md, aliasDecl->getStart()->getLine() - 1));
    }

    // 16) Extended type base name (Vec, Map, Set, Task) — normalize lowercase keywords
    auto* extDesc = extTypeRegistry_.lookup(normalizeExtBaseName(name));
    if (extDesc) {
        std::ostringstream ss;
        ss << "```lux\n(type) " << name;
        if (extDesc->genericArity == 1) ss << "<T>";
        else if (extDesc->genericArity == 2) ss << "<K, V>";
        ss << "\n```\n\n**Methods:**\n```lux\n";
        for (auto& m : extDesc->methods) {
            ss << m.returnType << " " << m.name << "(";
            for (size_t i = 0; i < m.paramTypes.size(); i++) {
                if (i > 0) ss << ", ";
                ss << m.paramTypes[i];
            }
            ss << ")\n";
        }
        ss << "```";
        return makeResult(token, ss.str());
    }

    // 17) Primitive type used as identifier (should not normally happen)
    auto* typeInfo = typeRegistry_.lookup(name);
    if (typeInfo) {
        std::string md = "```lux\n(type) " + typeInfo->name + "\n```";
        return makeResult(token, md);
    }

    // 18) Enum variant from `use EnumType::*;`
    auto tryEnumWildcardHover = [&](LuxParser::UseDeclContext* useDecl) -> std::optional<HoverResult> {
        auto* ew = dynamic_cast<LuxParser::UseEnumWildcardContext*>(useDecl);
        if (!ew) return std::nullopt;
        auto baseName = extractBaseTypeName(ew->typeSpec());
        if (baseName.empty()) return std::nullopt;
        auto* ed = findEnumDecl(tree, baseName);
        if (!ed) return std::nullopt;
        for (auto* variant : ed->enumVariant()) {
            auto* v = variant->IDENTIFIER();
            if (v && v->getText() == name) {
                std::string md = "```lux\n(variant) " + baseName + "::" + name + "\n```";
                return makeResult(token, withDoc(md, ed->getStart()->getLine() - 1));
            }
        }
        return std::nullopt;
    };
    for (auto* pre : tree->preambleDecl()) {
        auto* useDecl = pre->useDecl();
        if (!useDecl) continue;
        auto r = tryEnumWildcardHover(useDecl);
        if (r) return r;
    }
    // Also check in-function use EnumType::*; statements before cursor
    auto* enclosingFunc = findEnclosingFunction(tree, cursorLine);
    if (enclosingFunc && enclosingFunc->block()) {
        for (auto* stmt : enclosingFunc->block()->statement()) {
            auto* start = stmt->getStart();
            if (start && start->getLine() - 1 > cursorLine) break;
            if (auto* useDecl = stmt->useDecl()) {
                auto r = tryEnumWildcardHover(useDecl);
                if (r) return r;
            }
        }
    }

    return std::nullopt;
}

std::optional<HoverResult> HoverProvider::hoverGenericIntrinsic(
        LuxParser::GenericQualifiedFnCallExprContext* ctx,
        antlr4::Token* token) {
    auto ids = ctx->IDENTIFIER();
    for (size_t i = 0; i < ids.size(); i++) {
        if (ids[i]->getSymbol() == token) {
            // Se for o último, é a função
            if (i == ids.size() - 1) {
                std::vector<std::string> idTexts;
                for (auto* id : ids) idTexts.push_back(id->getText());
                std::string ns, funcName;
                if (IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName)) {
                    auto* intrinsic = intrinsicRegistry_.lookup(ns, funcName);
                    if (intrinsic) {
                         std::string md = "```lux\n(intrinsic) " + ns + "::" + funcName + "\n```\n\n" + intrinsic->description;
                         return makeResult(token, md);
                    }
                }
            } else {
                // Se não, é namespace
                std::string nsPath;
                for (size_t j = 0; j <= i; j++) {
                    if (j > 0) nsPath += "::";
                    nsPath += ids[j]->getText();
                }
                std::string md = "```lux\n(intrinsic namespace) " + nsPath + "\n```";
                return makeResult(token, md);
            }
        }
    }
    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  Hover on imported symbol in `use` declaration
// ═══════════════════════════════════════════════════════════════════════

std::optional<HoverResult> HoverProvider::hoverImportedSymbol(
        const std::string& symbolName,
        const std::string& modulePath,
        antlr4::Token* token,
        LuxParser::ProgramContext* tree,
        const CBindings& bindings,
        const ProjectContext* project) {
    std::string importedPath = modulePath;
    if (!importedPath.empty()) importedPath += "::";
    importedPath += symbolName;

    // 1. Se for stdlib: distinguir função e constante
    if (ImportResolver::isStdModule(modulePath)) {
        if (ImportResolver::moduleExportsSymbol(modulePath, symbolName)) {
            // Função do stdlib
            auto* builtin = builtinRegistry_.lookup(symbolName);
            if (builtin)
                return makeResult(token, formatBuiltinSignature(*builtin));
            // fallback: só nome
            std::string md = "```lux\n(function) " + symbolName + "\n```";
            return makeResult(token, md);
        }
        // Constante do stdlib
        auto& constType = builtinRegistry_.lookupConstant(symbolName);
        if (!constType.empty()) {
            std::string md = "```lux\n(constant) " + constType + " " + symbolName + "\n```";
            return makeResult(token, md);
        }
        // Se não for função nem constante, pode ser módulo
        if (ImportResolver::isStdModule(modulePath + "::" + symbolName)) {
            std::string md = "```lux\n(module) " + modulePath + "::" + symbolName + "\n```";
            return makeResult(token, md);
        }
    }

    // 2. Project registry (user code)
    if (project && project->isValid()) {
        auto* sym = project->registry().findSymbol(modulePath, symbolName);
        if (sym) {
            switch (sym->kind) {
                case ExportedSymbol::Function:
                    if (auto* fd = dynamic_cast<LuxParser::FunctionDeclContext*>(sym->decl))
                        return makeResult(token, formatFunctionDecl(fd));
                    break;
                case ExportedSymbol::Struct:
                    if (auto* sd = dynamic_cast<LuxParser::StructDeclContext*>(sym->decl))
                        return makeResult(token, formatStructDecl(sd));
                    break;
                case ExportedSymbol::Enum:
                    if (auto* ed = dynamic_cast<LuxParser::EnumDeclContext*>(sym->decl))
                        return makeResult(token, formatEnumDecl(ed));
                    break;
                case ExportedSymbol::Union:
                    if (auto* ud = dynamic_cast<LuxParser::UnionDeclContext*>(sym->decl))
                        return makeResult(token, formatUnionDecl(ud));
                    break;
                case ExportedSymbol::TypeAlias:
                    if (auto* ta = dynamic_cast<LuxParser::TypeAliasDeclContext*>(sym->decl)) {
                        std::string md = "```lux\ntype " + symbolName + " = "
                                       + typeSpecToString(ta->typeSpec()) + "\n```";
                        return makeResult(token, md);
                    }
                    break;
                case ExportedSymbol::ExtendBlock:
                    if (auto* ext = dynamic_cast<LuxParser::ExtendDeclContext*>(sym->decl))
                        return makeResult(token, formatExtendMethods(ext));
                    break;
            }
        }
    }

    // 3. Fallback: builtin função
    auto* builtin = builtinRegistry_.lookup(symbolName);
    if (builtin)
        return makeResult(token, formatBuiltinSignature(*builtin));

    // 4. Fallback: builtin constante
    auto& constType = builtinRegistry_.lookupConstant(symbolName);
    if (!constType.empty()) {
        std::string md = "```lux\n(constant) " + constType + " " + symbolName + "\n```";
        return makeResult(token, md);
    }

    // 5. Fallback: módulo
    if (ImportResolver::isStdModule(modulePath + "::" + symbolName)) {
        std::string md = "```lux\n(module) " + modulePath + "::" + symbolName + "\n```";
        return makeResult(token, md);
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  Recursive expression tree walker for hover
// ═══════════════════════════════════════════════════════════════════════

// Check if a token belongs to a parse tree node's token range.
static bool containsToken(antlr4::ParserRuleContext* ctx, antlr4::Token* token) {
    if (!ctx || !token) return false;
    auto* start = ctx->getStart();
    auto* stop  = ctx->getStop();
    if (!start || !stop) return false;
    size_t idx = token->getTokenIndex();
    return idx >= start->getTokenIndex() && idx <= stop->getTokenIndex();
}

static std::optional<std::string> resolveImportedModuleAliasPath(
        antlr4::Token* token,
        LuxParser::ProgramContext* tree) {
    if (!token || !tree) return std::nullopt;
    std::string name = token->getText();

    for (auto* pre : tree->preambleDecl()) {
        auto* useDecl = pre->useDecl();
        if (!useDecl) continue;

        if (auto* root = dynamic_cast<LuxParser::UseRootContext*>(useDecl)) {
            auto* id = root->IDENTIFIER();
            if (id && id->getText() == name)
                return id->getText();
        }

        if (auto* item = dynamic_cast<LuxParser::UseItemContext*>(useDecl)) {
            if (!item->modulePath()) continue;
            auto* importedId = item->IDENTIFIER();
            if (importedId && importedId->getText() == name) {
                std::string modulePath;
                for (auto* id : item->modulePath()->IDENTIFIER()) {
                    if (!modulePath.empty()) modulePath += "::";
                    modulePath += id->getText();
                }
                modulePath += "::";
                modulePath += importedId->getText();
                return modulePath;
            }
            auto ids = item->modulePath()->IDENTIFIER();
            if (ids.empty()) continue;
            auto* aliasId = ids.back();
            if (aliasId->getText() == name) {
                std::string modulePath;
                for (auto* id : ids) {
                    if (!modulePath.empty()) modulePath += "::";
                    modulePath += id->getText();
                }
                return modulePath;
            }
        }
    }
    return std::nullopt;
}

std::optional<HoverResult> HoverProvider::walkExprForHover(
        LuxParser::ExpressionContext* expr,
        antlr4::Token* hoveredToken,
        const std::string& tokenText,
        LuxParser::ProgramContext* tree,
        const CBindings& bindings,
        size_t cursorLine,
        const ProjectContext* project) {
    if (!expr || !containsToken(expr, hoveredToken)) return std::nullopt;

    // ── Method call: expr.method(args) ──────────────────────────────
    if (auto* mc = dynamic_cast<LuxParser::MethodCallExprContext*>(expr)) {
        auto* methodId = mc->IDENTIFIER();
        if (methodId && methodId->getSymbol() == hoveredToken) {
            return hoverMethodCall(mc, tree, bindings, cursorLine, project);
        }
        // Recurse into receiver and args
        auto r = walkExprForHover(mc->expression(), hoveredToken, tokenText,
                                   tree, bindings, cursorLine, project);
        if (r) return r;
        if (auto* al = mc->argList()) {
            for (auto* a : al->expression()) {
                r = walkExprForHover(a, hoveredToken, tokenText,
                                      tree, bindings, cursorLine, project);
                if (r) return r;
            }
        }
        return std::nullopt;
    }

    // ── Field access: expr.field ────────────────────────────────────
    if (auto* fa = dynamic_cast<LuxParser::FieldAccessExprContext*>(expr)) {
        auto* fieldId = fa->IDENTIFIER();
        if (fieldId && fieldId->getSymbol() == hoveredToken) {
            return hoverFieldAccess(fa, tree, bindings, cursorLine, project);
        }
        return walkExprForHover(fa->expression(), hoveredToken, tokenText,
                                 tree, bindings, cursorLine, project);
    }

    // ── Arrow access: expr->field (pointer dereference + field) ─────
    if (auto* aa = dynamic_cast<LuxParser::ArrowAccessExprContext*>(expr)) {
        auto* fieldId = aa->IDENTIFIER();
        if (fieldId && fieldId->getSymbol() == hoveredToken) {
            std::string fieldName = fieldId->getText();
            auto* receiver = aa->expression();
            auto* token = fieldId->getSymbol();

            std::string receiverTypeName;
            auto* func = findEnclosingFunction(tree, cursorLine);
            if (func) {
                auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
                FuncLookupCtx flc;
                flc.tree = tree;
                flc.bindings = &bindings;
                flc.builtinReg = &builtinRegistry_;
        flc.intrinsicReg = &intrinsicRegistry_;
                flc.extTypeReg = &extTypeRegistry_;
                flc.methodReg = &methodRegistry_;
                flc.project = project;
                receiverTypeName = inferExprTypeName(receiver, locals, &flc);
            }

            // Dereference one level for arrow access and normalize generic base.
            std::string structName = receiverTypeName;
            if (!structName.empty() && structName[0] == '*') {
                structName = structName.substr(1);
            }
            std::string base;
            std::vector<std::string> args;
            if (parseGenericInstance(structName, base, args)) {
                structName = base;
            }

            // Search local structs
            if (!structName.empty()) {
                auto* sd = findStructDecl(tree, structName);
                if (sd) {
                    for (auto* f : sd->structField()) {
                        if (safeText(f->IDENTIFIER()) == fieldName) {
                            std::string md = "```lux\n(field) "
                                + typeSpecToString(f->typeSpec()) + " "
                                + structName + "." + fieldName + "\n```";
                            return makeResult(token, md);
                        }
                    }
                }
            }

            // Search cross-file structs
            if (project && project->isValid()) {
                for (auto& ns : project->registry().allNamespaces()) {
                    auto syms = project->registry().getNamespaceSymbols(ns);
                    for (auto* sym : syms) {
                        if (sym->kind != ExportedSymbol::Struct) continue;
                        auto* sd = dynamic_cast<LuxParser::StructDeclContext*>(sym->decl);
                        if (!sd) continue;
                        if (!structName.empty() &&
                            safeText(sd->IDENTIFIER()) != structName)
                            continue;
                        for (auto* f : sd->structField()) {
                            if (safeText(f->IDENTIFIER()) == fieldName) {
                                std::string md = "```lux\n(field) "
                                    + typeSpecToString(f->typeSpec()) + " "
                                    + safeText(sd->IDENTIFIER()) + "."
                                    + fieldName + "\n```";
                                return makeResult(token, md);
                            }
                        }
                    }
                }
            }

            // Search C structs
            if (!structName.empty()) {
                auto* cs = bindings.findStruct(structName);
                if (cs) {
                    for (auto& f : cs->fields) {
                        if (f.name == fieldName) {
                            std::string md = "```c\n(field) " + f.typeInfo->name
                                + " " + structName + "." + fieldName
                                + "\n```\n*C struct field*";
                            return makeResult(token, md);
                        }
                    }
                }
            }

            return std::nullopt;
        }
        return walkExprForHover(aa->expression(), hoveredToken, tokenText,
                                 tree, bindings, cursorLine, project);
    }

    // ── Address-of: &expression ───────────────────────────────────
    if (auto* ao = dynamic_cast<LuxParser::AddrOfExprContext*>(expr)) {
        return walkExprForHover(ao->expression(), hoveredToken, tokenText,
                                 tree, bindings, cursorLine, project);
    }

    // ── Dereference: *expr ──────────────────────────────────────────
    if (auto* de = dynamic_cast<LuxParser::DerefExprContext*>(expr)) {
        return walkExprForHover(de->expression(), hoveredToken, tokenText,
                                 tree, bindings, cursorLine, project);
    }

    // ── Enum access: Type::Variant ──────────────────────────────────
    if (auto* ea = dynamic_cast<LuxParser::EnumAccessExprContext*>(expr)) {
        auto ids = ea->IDENTIFIER();
        if (ids.size() >= 2) {
            // Hover on variant name
            if (ids[1]->getSymbol() == hoveredToken) {
                return hoverEnumAccess(ea, tree, bindings, project);
            }
            // Hover on enum type name
            if (ids[0]->getSymbol() == hoveredToken) {
                std::string typeName = ids[0]->getText();
                return hoverIdent(typeName, hoveredToken, tree, bindings,
                                   cursorLine, project);
            }
        }
        return std::nullopt;
    }

    // ── Generic enum access: Type<T>::Variant ──────────────────────
    if (auto* gea = dynamic_cast<LuxParser::GenericEnumAccessExprContext*>(expr)) {
        auto ids = gea->IDENTIFIER();
        if (ids.size() >= 1 && ids[0]->getSymbol() == hoveredToken) {
            return hoverIdent(ids[0]->getText(), hoveredToken, tree, bindings,
                               cursorLine, project);
        }
        if (ids.size() >= 2 && ids[1]->getSymbol() == hoveredToken) {
            std::string baseName = ids[0]->getText();
            std::string variantName = ids[1]->getText();

            std::string fullType = baseName + "<";
            auto args = gea->typeSpec();
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) fullType += ", ";
                fullType += typeSpecToString(args[i]);
            }
            fullType += ">";

            // Validate against local enum declaration (generic base enum)
            auto* ed = findEnumDecl(tree, baseName);
            if (ed) {
                for (auto* variant : ed->enumVariant()) {
                    auto* v = variant->IDENTIFIER();
                    if (v && v->getText() == variantName) {
                        std::string md = "```lux\n(variant) " + fullType + "::" +
                                         variantName + "\n```";
                        size_t declLine = ed->getStart()->getLine();
                        md = appendDocToHover(md, docComments_, declLine);
                        return makeResult(hoveredToken, md);
                    }
                }
            }

            return makeResult(hoveredToken,
                "```lux\n(variant) " + fullType + "::" + variantName + "\n```");
        }
        return std::nullopt;
    }

    // ── Static method call: Type::method(args) ─────────────────────
    if (auto* smc = dynamic_cast<LuxParser::StaticMethodCallExprContext*>(expr)) {
        auto ids = smc->IDENTIFIER();
        if (ids.size() >= 2) {
            auto* methodTok = ids.back()->getSymbol();
            // Hover on method name
            if (methodTok == hoveredToken) {
                return hoverStaticMethodCall(smc, tree, bindings, cursorLine, project);
            }
            // Hover on owner scope/type path segments.
            for (size_t i = 0; i + 1 < ids.size(); ++i) {
                if (ids[i]->getSymbol() != hoveredToken) continue;
                std::string ownerPath;
                for (size_t j = 0; j + 1 < ids.size(); ++j) {
                    if (!ownerPath.empty()) ownerPath += "::";
                    ownerPath += ids[j]->getText();
                }

                // std::... scope chain
                if (ownerPath == "std" || ownerPath.rfind("std::", 0) == 0) {
                    return makeResult(hoveredToken,
                        "```lux\n(module) " + ownerPath + "\n```");
                }

                // lux::... intrinsic root/namespace
                if (IntrinsicRegistry::isIntrinsicPrefix(ids[0]->getText())) {
                    if (i == 0) {
                        return makeResult(hoveredToken,
                            "```lux\n(intrinsic root) lux\n```\n\n"
                            "Intrinsic namespace root. Contains low-level compiler "
                            "builtin functions like `lux::core::trap()`.");
                    }
                    std::string ns = ids[i]->getText();
                    if (intrinsicRegistry_.hasNamespace(ns)) {
                        return makeResult(hoveredToken,
                            "```lux\n(intrinsic namespace) lux::" + ns + "\n```\n\n" +
                            intrinsicRegistry_.namespaceDescription(ns));
                    }
                    return makeResult(hoveredToken,
                        "```lux\n(intrinsic namespace) " + ns + "\n```");
                }

                // Type/namespace: resolve only the first segment as type symbol.
                if (i == 0) {
                    if (auto modulePath = resolveImportedModuleAliasPath(hoveredToken, tree)) {
                        return makeResult(hoveredToken,
                            "```lux\n(module) " + *modulePath + "\n```"
                        );
                    }
                    std::string typeName = ids[0]->getText();
                    return hoverIdent(typeName, hoveredToken, tree, bindings,
                                      cursorLine, project);
                }
            }
        }
        if (auto* al = smc->argList()) {
            for (auto* a : al->expression()) {
                auto r = walkExprForHover(a, hoveredToken, tokenText,
                                           tree, bindings, cursorLine, project);
                if (r) return r;
            }
        }
        return std::nullopt;
    }

    // ── Function call: expr(args) ──────────────────────────────────
    if (auto* fc = dynamic_cast<LuxParser::FnCallExprContext*>(expr)) {
        if (auto* calleeIdent = dynamic_cast<LuxParser::IdentExprContext*>(fc->expression())) {
            if (calleeIdent->IDENTIFIER() && calleeIdent->IDENTIFIER()->getSymbol() == hoveredToken) {
                std::string funcName = safeText(calleeIdent->IDENTIFIER());
                auto buildCallHover = [&](LuxParser::FunctionDeclContext* fd) -> std::optional<HoverResult> {
                    if (!fd) return std::nullopt;
                    if (!fd->typeParamList()) {
                        return makeResult(hoveredToken,
                            withDoc(formatFunctionDecl(fd), fd->getStart()->getLine() - 1));
                    }

                    auto* encFunc = findEnclosingFunction(tree, cursorLine);
                    if (!encFunc) {
                        return makeResult(hoveredToken,
                            withDoc(formatFunctionDecl(fd), fd->getStart()->getLine() - 1));
                    }
                    auto locals = collectLocals(encFunc, cursorLine, tree, &bindings, project);

                    FuncLookupCtx flc;
                    flc.tree = tree;
                    flc.bindings = &bindings;
                    flc.builtinReg = &builtinRegistry_;
        flc.intrinsicReg = &intrinsicRegistry_;
                    flc.extTypeReg = &extTypeRegistry_;
                    flc.project = project;

                    std::unordered_set<std::string> tps;
                    for (auto* tp : fd->typeParamList()->typeParam()) {
                        auto ids = tp->IDENTIFIER();
                        if (!ids.empty()) tps.insert(ids[0]->getText());
                    }

                    std::vector<std::string> actualArgTypes;
                    if (auto* al = fc->argList()) {
                        for (auto* a : al->expression())
                            actualArgTypes.push_back(inferExprTypeName(a, locals, &flc));
                    }

                    std::vector<LuxParser::ParamContext*> formalParams;
                    if (auto* pl = fd->paramList()) formalParams = pl->param();
                    if (actualArgTypes.size() != formalParams.size()) {
                        return makeResult(hoveredToken,
                            withDoc(formatFunctionDecl(fd), fd->getStart()->getLine() - 1));
                    }

                    std::unordered_map<std::string, std::string> inferred;
                    bool ok = true;
                    for (size_t i = 0; i < formalParams.size(); i++) {
                        auto formal = safeText(formalParams[i]->typeSpec());
                        auto actual = actualArgTypes[i];
                        if (actual.empty()) { ok = false; break; }
                        if (!unifyGenericType(formal, actual, tps, inferred)) {
                            ok = false;
                            break;
                        }
                    }
                    for (auto& tp : tps) {
                        if (!inferred.count(tp)) { ok = false; break; }
                    }

                    if (!ok) {
                        return makeResult(hoveredToken,
                            withDoc(formatFunctionDecl(fd), fd->getStart()->getLine() - 1));
                    }

                    std::string retType = substituteTypeParams(safeText(fd->typeSpec()), inferred);
                    std::ostringstream ss;
                    ss << "```lux\n(function) " << retType << " " << funcName << "(";
                    bool first = true;
                    if (auto* params = fd->paramList()) {
                        for (auto* p : params->param()) {
                            if (!first) ss << ", ";
                            first = false;
                            ss << substituteTypeParams(safeText(p->typeSpec()), inferred)
                               << " " << safeText(p->IDENTIFIER());
                        }
                    }
                    ss << ")\n```";
                    return makeResult(hoveredToken, withDoc(ss.str(), fd->getStart()->getLine() - 1));
                };

                for (auto* tld : tree->topLevelDecl()) {
                    auto* fd = tld->functionDecl();
                    if (!fd || fd->IDENTIFIER().empty() || safeIdAt(fd, 0) != funcName)
                        continue;
                    if (auto h = buildCallHover(fd)) return h;
                }
                if (project && project->isValid()) {
                    for (auto& ns : project->registry().allNamespaces()) {
                        auto* sym = project->registry().findSymbol(ns, funcName);
                        if (!sym || sym->kind != ExportedSymbol::Function) continue;
                        auto* fd = dynamic_cast<LuxParser::FunctionDeclContext*>(sym->decl);
                        if (auto h = buildCallHover(fd)) return h;
                    }
                }
            }
        }

        auto r = walkExprForHover(fc->expression(), hoveredToken, tokenText,
                                   tree, bindings, cursorLine, project);
        if (r) return r;
        if (auto* al = fc->argList()) {
            for (auto* a : al->expression()) {
                r = walkExprForHover(a, hoveredToken, tokenText,
                                      tree, bindings, cursorLine, project);
                if (r) return r;
            }
        }
        return std::nullopt;
    }

    // ── Struct literal: Type { field: expr, ... } ────────────────────
    if (auto* sl = dynamic_cast<LuxParser::StructLitExprContext*>(expr)) {
        auto ids = sl->IDENTIFIER();
        if (!ids.empty()) {
            // Hover on struct type name
            if (ids[0]->getSymbol() == hoveredToken) {
                return hoverIdent(ids[0]->getText(), hoveredToken, tree,
                                   bindings, cursorLine, project);
            }
            // Hover on field names
            std::string structName = ids[0]->getText();
            for (size_t i = 1; i < ids.size(); i++) {
                if (ids[i]->getSymbol() != hoveredToken) continue;
                std::string fieldName = ids[i]->getText();
                // Look up field type from struct declaration
                auto* sd = findStructDecl(tree, structName);
                if (sd) {
                    for (auto* f : sd->structField()) {
                        if (safeText(f->IDENTIFIER()) == fieldName) {
                            std::string md = "```lux\n(field) "
                                + typeSpecToString(f->typeSpec()) + " "
                                + structName + "." + fieldName + "\n```";
                            return makeResult(hoveredToken, md);
                        }
                    }
                }
                // Cross-file struct fields
                if (project && project->isValid()) {
                    for (auto& ns : project->registry().allNamespaces()) {
                        auto syms = project->registry().getNamespaceSymbols(ns);
                        for (auto* sym : syms) {
                            if (sym->kind != ExportedSymbol::Struct ||
                                sym->name != structName) continue;
                            auto* rsd = dynamic_cast<LuxParser::StructDeclContext*>(sym->decl);
                            if (!rsd) continue;
                            for (auto* f : rsd->structField()) {
                                if (safeText(f->IDENTIFIER()) == fieldName) {
                                    std::string md = "```lux\n(field) "
                                        + typeSpecToString(f->typeSpec()) + " "
                                        + structName + "." + fieldName + "\n```";
                                    return makeResult(hoveredToken, md);
                                }
                            }
                        }
                    }
                }
                // Union fields
                auto* ud = findUnionDecl(tree, structName);
                if (ud) {
                    for (auto* f : ud->unionField()) {
                        if (safeText(f->IDENTIFIER()) == fieldName) {
                            std::string md = "```lux\n(field) "
                                + typeSpecToString(f->typeSpec()) + " "
                                + structName + "." + fieldName + "\n```";
                            return makeResult(hoveredToken, md);
                        }
                    }
                }
                // C struct fields
                if (auto* cs = bindings.findStruct(structName)) {
                    for (auto& f : cs->fields) {
                        if (f.name == fieldName) {
                            std::string md = "```lux\n(field) "
                                + f.typeInfo->name + " " + structName + "."
                                + fieldName + "\n```";
                            return makeResult(hoveredToken, md);
                        }
                    }
                }
                // Built-in structs from TypeRegistry (e.g. Error)
                auto* bti = typeRegistry_.lookup(structName);
                if (bti && bti->kind == TypeKind::Struct) {
                    for (auto& f : bti->fields) {
                        if (f.name == fieldName) {
                            std::string md = "```lux\n(field) "
                                + f.typeInfo->name + " " + structName + "."
                                + fieldName + "\n```";
                            return makeResult(hoveredToken, md);
                        }
                    }
                }
            }
        }
        // Recurse into initializer expressions
        for (auto* child : sl->expression()) {
            auto r = walkExprForHover(child, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
        return std::nullopt;
    }

    // ── Generic function call: max<int32>(a, b) ─────────────────────
    if (auto* gfc = dynamic_cast<LuxParser::GenericFnCallExprContext*>(expr)) {
        auto* fnId = gfc->IDENTIFIER();
        if (fnId && fnId->getSymbol() == hoveredToken) {
            std::string funcName = fnId->getText();
            for (auto* tld : tree->topLevelDecl()) {
                auto* fd = tld->functionDecl();
                if (!fd || fd->IDENTIFIER().empty() || safeIdAt(fd, 0) != funcName)
                    continue;

                if (!fd->typeParamList()) {
                    return makeResult(hoveredToken,
                        withDoc(formatFunctionDecl(fd), fd->getStart()->getLine() - 1));
                }

                std::unordered_map<std::string, std::string> subst;
                auto tps = fd->typeParamList()->typeParam();
                auto args = gfc->typeSpec();
                for (size_t i = 0; i < std::min(tps.size(), args.size()); i++) {
                    auto ids = tps[i]->IDENTIFIER();
                    if (!ids.empty())
                        subst[ids[0]->getText()] = args[i]->getText();
                }

                std::string retType = substituteTypeParams(safeText(fd->typeSpec()), subst);
                std::ostringstream ss;
                ss << "```lux\n(function) " << retType << " " << funcName << "(";
                bool first = true;
                if (auto* pl = fd->paramList()) {
                    for (auto* p : pl->param()) {
                        if (!first) ss << ", ";
                        first = false;
                        ss << substituteTypeParams(safeText(p->typeSpec()), subst)
                           << " " << safeText(p->IDENTIFIER());
                    }
                }
                ss << ")\n```";
                return makeResult(hoveredToken, withDoc(ss.str(), fd->getStart()->getLine() - 1));
            }

            return hoverIdent(fnId->getText(), hoveredToken, tree, bindings,
                               cursorLine, project);
        }
        if (auto* al = gfc->argList()) {
            for (auto* a : al->expression()) {
                auto r = walkExprForHover(a, hoveredToken, tokenText,
                                           tree, bindings, cursorLine, project);
                if (r) return r;
            }
        }
        return std::nullopt;
    }

    if (auto* gqfc = dynamic_cast<LuxParser::GenericQualifiedFnCallExprContext*>(expr)) {
        if (gqfc->IDENTIFIER().back()->getSymbol() == hoveredToken) {
            return hoverGenericIntrinsic(gqfc, hoveredToken);
        }
    }

    // ── Generic static method call: Node<int32>::create(42) ─────────
    if (auto* gsmc = dynamic_cast<LuxParser::GenericStaticMethodCallExprContext*>(expr)) {
        auto ids = gsmc->IDENTIFIER();
        if (ids.size() >= 1 && ids[0]->getSymbol() == hoveredToken) {
            if (auto modulePath = resolveImportedModuleAliasPath(hoveredToken, tree)) {
                return makeResult(hoveredToken,
                    "```lux\n(module) " + *modulePath + "\n```"
                );
            }
            return hoverIdent(ids[0]->getText(), hoveredToken, tree, bindings,
                               cursorLine, project);
        }
        if (ids.size() >= 2 && ids[1]->getSymbol() == hoveredToken) {
            // Show the generic struct template name + method info
            std::string structName = ids[0]->getText();
            std::string methodName = ids[1]->getText();
            auto* ext = findExtendDecl(tree, structName);
            if (ext) {
                for (auto* m : ext->extendMethod()) {
                    if (m->IDENTIFIER().empty()) continue;
                    if (safeIdAt(m, 0) == methodName) {
                        std::ostringstream ss;
                        ss << "```lux\n(static method) " << typeSpecToString(m->typeSpec())
                           << " " << structName << "<T>::" << methodName << "(";
                        if (auto* params = m->paramList()) {
                            bool first = true;
                            for (auto* p : params->param()) {
                                if (!first) ss << ", ";
                                first = false;
                                ss << typeSpecToString(p->typeSpec())
                                   << " " << safeText(p->IDENTIFIER());
                            }
                        }
                        ss << ")\n```";
                        return makeResult(hoveredToken, ss.str());
                    }
                }
            }
            return makeResult(hoveredToken,
                "```lux\n(method) " + structName + "::" + methodName + "()\n```");
        }
        if (auto* al = gsmc->argList()) {
            for (auto* a : al->expression()) {
                auto r = walkExprForHover(a, hoveredToken, tokenText,
                                           tree, bindings, cursorLine, project);
                if (r) return r;
            }
        }
        return std::nullopt;
    }

    // ── Generic struct literal: Node<int32> { field: val } ──────────
    if (auto* gsl = dynamic_cast<LuxParser::GenericStructLitExprContext*>(expr)) {
        auto ids = gsl->IDENTIFIER();
        if (!ids.empty() && ids[0]->getSymbol() == hoveredToken) {
            return hoverIdent(ids[0]->getText(), hoveredToken, tree, bindings,
                               cursorLine, project);
        }
        if (ids.size() > 1) {
            std::string structName = ids[0]->getText();
            auto* sd = findStructDecl(tree, structName);
            for (size_t i = 1; i < ids.size(); i++) {
                if (ids[i]->getSymbol() != hoveredToken) continue;
                std::string fieldName = ids[i]->getText();
                if (sd) {
                    for (auto* f : sd->structField()) {
                        if (safeText(f->IDENTIFIER()) == fieldName) {
                            std::string md = "```lux\n(field) "
                                + typeSpecToString(f->typeSpec()) + " "
                                + structName + "." + fieldName + "\n```";
                            return makeResult(hoveredToken, md);
                        }
                    }
                }
                return makeResult(hoveredToken,
                    "```lux\n(field) " + structName + "." + fieldName + "\n```");
            }
        }
        for (auto* child : gsl->expression()) {
            auto r = walkExprForHover(child, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
        return std::nullopt;
    }

    // ── Cast: expr as Type ──────────────────────────────────────────
    if (auto* cast = dynamic_cast<LuxParser::CastExprContext*>(expr)) {
        if (auto* ts = cast->typeSpec()) {
            auto r = hoverTypeSpec(ts, hoveredToken, tree, bindings, project);
            if (r) return r;
        }
        return walkExprForHover(cast->expression(), hoveredToken, tokenText,
                                 tree, bindings, cursorLine, project);
    }

    // ── Is: expr is Type [::Variant] ────────────────────────────────
    if (auto* isE = dynamic_cast<LuxParser::IsExprContext*>(expr)) {
        if (auto* ts = isE->typeSpec()) {
            auto r = hoverTypeSpec(ts, hoveredToken, tree, bindings, project);
            if (r) return r;
        }

        if (isE->SCOPE() && isE->IDENTIFIER(0) &&
            isE->IDENTIFIER(0)->getSymbol() == hoveredToken) {
            auto variantName = safeIdAt(isE, 0);
            auto* ts = isE->typeSpec();
            std::string fullType = ts ? typeSpecToString(ts) : std::string("?");

            std::string md = "```lux\n(variant) " + fullType + "::" +
                             variantName + "\n```";
            return makeResult(hoveredToken, md);
        }

        if (isE->LPAREN() && isE->IDENTIFIER(1) &&
            isE->IDENTIFIER(1)->getSymbol() == hoveredToken) {
            std::string bindType = "auto";
            if (auto* ts = isE->typeSpec(); ts && ts->IDENTIFIER() && isE->IDENTIFIER(0)) {
                auto enumName = safeText(ts->IDENTIFIER());
                auto variantName = safeIdAt(isE, 0);
                if (auto* ed = findEnumDecl(tree, enumName)) {
                    for (auto* variant : ed->enumVariant()) {
                        auto* vId = variant->IDENTIFIER();
                        if (!vId || vId->getText() != variantName) continue;
                        if (variant->LPAREN() && variant->typeSpec().size() == 1) {
                            bindType = variant->typeSpec(0)->getText();
                            if (ts->LT() && ed->typeParamList()) {
                                std::unordered_map<std::string, std::string> subst;
                                auto tps = ed->typeParamList()->typeParam();
                                auto args = ts->typeSpec();
                                for (size_t i = 0; i < tps.size() && i < args.size(); i++) {
                                    auto ids = tps[i]->IDENTIFIER();
                                    if (!ids.empty()) subst[ids[0]->getText()] = args[i]->getText();
                                }
                                bindType = substituteTypeParams(bindType, subst);
                            }
                        }
                        break;
                    }
                }
            }

            std::string md = "```lux\n(variable) " + bindType + " " +
                             safeIdAt(isE, 1) + "\n```";
            return makeResult(hoveredToken, md);
        }

        return walkExprForHover(isE->expression(), hoveredToken, tokenText,
                                 tree, bindings, cursorLine, project);
    }

    // ── Sizeof: sizeof(Type) ────────────────────────────────────────
    if (auto* sz = dynamic_cast<LuxParser::SizeofExprContext*>(expr)) {
        if (sz->SIZEOF() && sz->SIZEOF()->getSymbol() == hoveredToken) {
            return makeResult(hoveredToken,
                "```lux\n(keyword) int64 sizeof(type)\n```\n"
                "Returns the size in bytes of the given type.");
        }
        if (auto* ts = sz->typeSpec()) {
            auto r = hoverTypeSpec(ts, hoveredToken, tree, bindings, project);
            if (r) return r;
        }
        return std::nullopt;
    }

    // ── Typeof: typeof(expr) ────────────────────────────────────────
    if (auto* to = dynamic_cast<LuxParser::TypeofExprContext*>(expr)) {
        if (to->TYPEOF() && to->TYPEOF()->getSymbol() == hoveredToken) {
            return makeResult(hoveredToken,
                "```lux\n(keyword) string typeof(expression)\n```\n"
                "Returns the type name of the given expression as a string.");
        }
        return walkExprForHover(to->expression(), hoveredToken, tokenText,
                                 tree, bindings, cursorLine, project);
    }

    // ── List comprehension: [expr | for Type x in range if cond] ────
    if (auto* lc = dynamic_cast<LuxParser::ListCompExprContext*>(expr)) {
        // Hover on iterator type spec
        if (auto* ts = lc->typeSpec()) {
            auto r = hoverTypeSpec(ts, hoveredToken, tree, bindings, project);
            if (r) return r;
        }
        // Hover on iterator variable name
        if (auto* id = lc->IDENTIFIER()) {
            if (id->getSymbol() == hoveredToken) {
                std::string typeName = typeSpecToString(lc->typeSpec());
                std::string md = "```lux\n(variable) " + typeName + " "
                               + id->getText() + "\n```\n*List comprehension iterator*";
                return makeResult(hoveredToken, md);
            }
        }
        // Hover on 'for' keyword
        if (lc->FOR() && lc->FOR()->getSymbol() == hoveredToken) {
            return makeResult(hoveredToken,
                "```lux\n(keyword) for\n```\n"
                "List comprehension iterator clause.");
        }
        // Recurse into all sub-expressions (transform, range, filter)
        for (auto* e : lc->expression()) {
            auto r = walkExprForHover(e, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
        return std::nullopt;
    }

    // ── Index access: expr[expr] ────────────────────────────────────
    if (auto* idx = dynamic_cast<LuxParser::IndexExprContext*>(expr)) {
        auto exprs = idx->expression();
        // Recurse into base expression (exprs[0]) and index (exprs[1])
        for (auto* e : exprs) {
            auto r = walkExprForHover(e, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
        return std::nullopt;
    }

    // ── Tuple index access: expr.0, expr.1 ─────────────────────────
    if (auto* ti = dynamic_cast<LuxParser::TupleIndexExprContext*>(expr)) {
        // If hovering on the INTEGER token (.0, .1 ...), show element type
        if (ti->INT_LIT() && ti->INT_LIT()->getSymbol() == hoveredToken) {
            auto* func = findEnclosingFunction(tree, cursorLine);
            if (func) {
                auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
                FuncLookupCtx flc;
                flc.tree = tree; flc.bindings = &bindings;
                flc.builtinReg = &builtinRegistry_;
                flc.intrinsicReg = &intrinsicRegistry_;
                flc.extTypeReg = &extTypeRegistry_;
                flc.project = project;
                std::string typeName = inferExprTypeName(expr, locals, &flc);
                if (!typeName.empty()) {
                    std::string idx = ti->INT_LIT()->getText();
                    std::string md = "```lux\n(tuple element) ." + idx + ": " + typeName + "\n```";
                    return makeResult(hoveredToken, md);
                }
            }
        }
        return walkExprForHover(ti->expression(), hoveredToken, tokenText,
                                 tree, bindings, cursorLine, project);
    }

    // ── Tuple arrow index: expr->0, expr->1 ────────────────────────
    if (auto* tai = dynamic_cast<LuxParser::TupleArrowIndexExprContext*>(expr)) {
        if (tai->INT_LIT() && tai->INT_LIT()->getSymbol() == hoveredToken) {
            auto* func = findEnclosingFunction(tree, cursorLine);
            if (func) {
                auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
                FuncLookupCtx flc;
                flc.tree = tree; flc.bindings = &bindings;
                flc.builtinReg = &builtinRegistry_;
                flc.intrinsicReg = &intrinsicRegistry_;
                flc.extTypeReg = &extTypeRegistry_;
                flc.project = project;
                std::string typeName = inferExprTypeName(expr, locals, &flc);
                if (!typeName.empty()) {
                    std::string idx = tai->INT_LIT()->getText();
                    std::string md = "```lux\n(tuple element) ->" + idx + ": " + typeName + "\n```";
                    return makeResult(hoveredToken, md);
                }
            }
        }
        return walkExprForHover(tai->expression(), hoveredToken, tokenText,
                                 tree, bindings, cursorLine, project);
    }

    // ── Chained tuple index: expr.N.M (FLOAT_LIT) ─────────────────
    if (auto* cti = dynamic_cast<LuxParser::ChainedTupleIndexExprContext*>(expr)) {
        if (cti->FLOAT_LIT() && cti->FLOAT_LIT()->getSymbol() == hoveredToken) {
            auto* func = findEnclosingFunction(tree, cursorLine);
            if (func) {
                auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
                FuncLookupCtx flc;
                flc.tree = tree; flc.bindings = &bindings;
                flc.builtinReg = &builtinRegistry_;
                flc.intrinsicReg = &intrinsicRegistry_;
                flc.extTypeReg = &extTypeRegistry_;
                flc.project = project;
                std::string typeName = inferExprTypeName(expr, locals, &flc);
                if (!typeName.empty()) {
                    std::string idx = cti->FLOAT_LIT()->getText();
                    std::string md = "```lux\n(tuple element) ." + idx + ": " + typeName + "\n```";
                    return makeResult(hoveredToken, md);
                }
            }
        }
        return walkExprForHover(cti->expression(), hoveredToken, tokenText,
                                 tree, bindings, cursorLine, project);
    }

    // ── Chained tuple arrow index: expr->N.M (FLOAT_LIT) ──────────
    if (auto* ctai = dynamic_cast<LuxParser::ChainedTupleArrowIndexExprContext*>(expr)) {
        if (ctai->FLOAT_LIT() && ctai->FLOAT_LIT()->getSymbol() == hoveredToken) {
            auto* func = findEnclosingFunction(tree, cursorLine);
            if (func) {
                auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
                FuncLookupCtx flc;
                flc.tree = tree; flc.bindings = &bindings;
                flc.builtinReg = &builtinRegistry_;
                flc.intrinsicReg = &intrinsicRegistry_;
                flc.extTypeReg = &extTypeRegistry_;
                flc.project = project;
                std::string typeName = inferExprTypeName(expr, locals, &flc);
                if (!typeName.empty()) {
                    std::string idx = ctai->FLOAT_LIT()->getText();
                    std::string md = "```lux\n(tuple element) ->" + idx + ": " + typeName + "\n```";
                    return makeResult(hoveredToken, md);
                }
            }
        }
        return walkExprForHover(ctai->expression(), hoveredToken, tokenText,
                                 tree, bindings, cursorLine, project);
    }

    // ── Tuple literal: (expr, expr, ...) ────────────────────────────
    if (auto* tl = dynamic_cast<LuxParser::TupleLitExprContext*>(expr)) {
        for (auto* e : tl->expression()) {
            auto r = walkExprForHover(e, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
        return std::nullopt;
    }

    // ── Catch unwrap expression: value catch { ... } ─────────────────
    if (auto* cu = dynamic_cast<LuxParser::CatchUnwrapExprContext*>(expr)) {
        auto r = walkExprForHover(cu->expression(), hoveredToken, tokenText,
                                  tree, bindings, cursorLine, project);
        if (r) return r;
        if (cu->block() && containsToken(cu->block(), hoveredToken)) {
            r = walkBlockForHover(cu->block(), hoveredToken, tokenText,
                                  tree, bindings, cursorLine, project);
            if (r) return r;
        }
        return std::nullopt;
    }

    // ── Array literal: [expr, expr, ...] ────────────────────────────
    if (auto* al = dynamic_cast<LuxParser::ArrayLitExprContext*>(expr)) {
        for (auto* e : al->expression()) {
            auto r = walkExprForHover(e, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
        return std::nullopt;
    }

    // ── Plain identifier ─────────────────────────────────────────────
    // Must come before the generic child-recurse so that cursorLine
    // (which may have been adjusted by the #scope callback handler to
    // the block's end line) is honoured instead of falling through to
    // the provide() fallback that uses the raw cursor position.
    if (auto* id = dynamic_cast<LuxParser::IdentExprContext*>(expr)) {
        if (id->IDENTIFIER()->getSymbol() == hoveredToken)
            return hoverIdent(tokenText, hoveredToken, tree, bindings,
                               cursorLine, project);
        return std::nullopt;
    }

    // ── Generic: recurse into children ─────────────────────────────
    for (auto* child : expr->children) {
        if (auto* childExpr = dynamic_cast<LuxParser::ExpressionContext*>(child)) {
            auto r = walkExprForHover(childExpr, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    return std::nullopt;
}

std::optional<HoverResult> HoverProvider::walkStmtForHover(
        LuxParser::StatementContext* stmt,
        antlr4::Token* hoveredToken,
        const std::string& tokenText,
        LuxParser::ProgramContext* tree,
        const CBindings& bindings,
        size_t cursorLine,
        const ProjectContext* project) {
    if (!stmt || !containsToken(stmt, hoveredToken)) return std::nullopt;

    // Check typeSpec in variable declarations
    if (auto* vd = stmt->varDeclStmt()) {
        if (auto* ts = vd->typeSpec()) {
            auto r = hoverTypeSpec(ts, hoveredToken, tree, bindings, project);
            if (r) return r;
        }
        // Check the initializer expression
        if (auto* initExpr = vd->expression()) {
            auto r = walkExprForHover(initExpr, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    // Check expressions in expression statements
    if (auto* es = stmt->exprStmt()) {
        auto r = walkExprForHover(es->expression(), hoveredToken, tokenText,
                                   tree, bindings, cursorLine, project);
        if (r) return r;
    }

    // Check expressions in return statements
    if (auto* rs = stmt->returnStmt()) {
        if (auto* e = rs->expression()) {
            auto r = walkExprForHover(e, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    // Check assignment statements
    if (auto* as = stmt->assignStmt()) {
        for (auto* e : as->expression()) {
            auto r = walkExprForHover(e, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    // Check field assignment statements: obj.field = expr;
    if (auto* fas = stmt->fieldAssignStmt()) {
        auto ids = fas->IDENTIFIER();
        if (!ids.empty() && ids[0] && ids[0]->getSymbol() == hoveredToken) {
            return hoverIdent(ids[0]->getText(), hoveredToken, tree,
                              bindings, cursorLine, project);
        }

        auto resolveFieldType = [&](const std::string& ownerType,
                                    const std::string& fieldName) -> std::string {
            if (ownerType.empty()) return "";

            std::string lookupType = ownerType;
            std::unordered_map<std::string, std::string> subst;
            std::string base;
            std::vector<std::string> args;
            if (parseGenericInstance(ownerType, base, args)) {
                lookupType = base;
            }

            // Same-file structs
            if (auto* sd = findStructDecl(tree, lookupType)) {
                if (!args.empty() && sd->typeParamList()) {
                    auto tps = sd->typeParamList()->typeParam();
                    for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
                        auto ids = tps[i]->IDENTIFIER();
                        if (!ids.empty()) subst[ids[0]->getText()] = args[i];
                    }
                }
                for (auto* f : sd->structField()) {
                    if (safeText(f->IDENTIFIER()) == fieldName)
                        return substituteTypeParams(typeSpecToString(f->typeSpec()), subst);
                }
            }
            // Same-file unions
            if (auto* ud = findUnionDecl(tree, lookupType)) {
                if (!args.empty() && ud->typeParamList()) {
                    auto tps = ud->typeParamList()->typeParam();
                    for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
                        auto ids = tps[i]->IDENTIFIER();
                        if (!ids.empty()) subst[ids[0]->getText()] = args[i];
                    }
                }
                for (auto* f : ud->unionField()) {
                    if (safeText(f->IDENTIFIER()) == fieldName)
                        return substituteTypeParams(typeSpecToString(f->typeSpec()), subst);
                }
            }
            // Built-in struct types
            if (auto* ti = typeRegistry_.lookup(ownerType)) {
                if (ti->kind == TypeKind::Struct || ti->kind == TypeKind::Union) {
                    for (auto& f : ti->fields) {
                        if (f.name == fieldName && f.typeInfo)
                            return f.typeInfo->name;
                    }
                }
            }
            // C structs
            if (auto* cs = bindings.findStruct(ownerType)) {
                for (auto& f : cs->fields) {
                    if (f.name == fieldName && f.typeInfo)
                        return f.typeInfo->name;
                }
            }
            return "";
        };

        if (ids.size() >= 2) {
            std::string currentType;
            auto* func = findEnclosingFunction(tree, cursorLine);
            if (func) {
                auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
                auto it = locals.find(ids[0]->getText());
                if (it != locals.end()) currentType = it->second.typeName;
            }

            for (size_t i = 1; i < ids.size(); i++) {
                auto* fid = ids[i];
                if (!fid) continue;
                std::string fieldName = fid->getText();
                std::string fieldType = resolveFieldType(currentType, fieldName);

                if (fid->getSymbol() == hoveredToken && !currentType.empty() && !fieldType.empty()) {
                    std::string md = "```lux\n(field) " + fieldType + " "
                                   + currentType + "." + fieldName + "\n```";
                    return makeResult(fid->getSymbol(), md);
                }

                if (fieldType.empty()) break;
                currentType = fieldType;
            }
        }

        if (auto* rhs = fas->expression()) {
            auto r = walkExprForHover(rhs, hoveredToken, tokenText,
                                      tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    // Check field compound assignment statements: obj.field += expr;
    if (auto* fcs = stmt->fieldCompoundAssignStmt()) {
        auto ids = fcs->IDENTIFIER();
        if (!ids.empty() && ids[0] && ids[0]->getSymbol() == hoveredToken) {
            return hoverIdent(ids[0]->getText(), hoveredToken, tree,
                              bindings, cursorLine, project);
        }

        auto resolveFieldType = [&](const std::string& ownerType,
                                    const std::string& fieldName) -> std::string {
            if (ownerType.empty()) return "";

            std::string lookupType = ownerType;
            std::unordered_map<std::string, std::string> subst;
            std::string base;
            std::vector<std::string> args;
            if (parseGenericInstance(ownerType, base, args)) {
                lookupType = base;
            }

            if (auto* sd = findStructDecl(tree, lookupType)) {
                if (!args.empty() && sd->typeParamList()) {
                    auto tps = sd->typeParamList()->typeParam();
                    for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
                        auto ids = tps[i]->IDENTIFIER();
                        if (!ids.empty()) subst[ids[0]->getText()] = args[i];
                    }
                }
                for (auto* f : sd->structField()) {
                    if (safeText(f->IDENTIFIER()) == fieldName)
                        return substituteTypeParams(typeSpecToString(f->typeSpec()), subst);
                }
            }
            if (auto* ud = findUnionDecl(tree, lookupType)) {
                if (!args.empty() && ud->typeParamList()) {
                    auto tps = ud->typeParamList()->typeParam();
                    for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
                        auto ids = tps[i]->IDENTIFIER();
                        if (!ids.empty()) subst[ids[0]->getText()] = args[i];
                    }
                }
                for (auto* f : ud->unionField()) {
                    if (safeText(f->IDENTIFIER()) == fieldName)
                        return substituteTypeParams(typeSpecToString(f->typeSpec()), subst);
                }
            }
            if (auto* ti = typeRegistry_.lookup(ownerType)) {
                if (ti->kind == TypeKind::Struct || ti->kind == TypeKind::Union) {
                    for (auto& f : ti->fields) {
                        if (f.name == fieldName && f.typeInfo)
                            return f.typeInfo->name;
                    }
                }
            }
            if (auto* cs = bindings.findStruct(ownerType)) {
                for (auto& f : cs->fields) {
                    if (f.name == fieldName && f.typeInfo)
                        return f.typeInfo->name;
                }
            }
            return "";
        };

        if (ids.size() >= 2) {
            std::string currentType;
            auto* func = findEnclosingFunction(tree, cursorLine);
            if (func) {
                auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
                auto it = locals.find(ids[0]->getText());
                if (it != locals.end()) currentType = it->second.typeName;
            }

            for (size_t i = 1; i < ids.size(); i++) {
                auto* fid = ids[i];
                if (!fid) continue;
                std::string fieldName = fid->getText();
                std::string fieldType = resolveFieldType(currentType, fieldName);

                if (fid->getSymbol() == hoveredToken && !currentType.empty() && !fieldType.empty()) {
                    std::string md = "```lux\n(field) " + fieldType + " "
                                   + currentType + "." + fieldName + "\n```";
                    return makeResult(fid->getSymbol(), md);
                }

                if (fieldType.empty()) break;
                currentType = fieldType;
            }
        }

        if (auto* rhs = fcs->expression()) {
            auto r = walkExprForHover(rhs, hoveredToken, tokenText,
                                      tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    // Check arrow assignment statements: ptr->field = expr;
    if (auto* aas = stmt->arrowAssignStmt()) {
        auto ids = aas->IDENTIFIER();
        if (ids.empty()) return std::nullopt;
        auto* baseId = ids[0];
        auto* fieldId = ids.size() >= 2 ? ids.back() : nullptr;

        // Hover on base identifier (e.g., self)
        if (baseId && baseId->getSymbol() == hoveredToken) {
            return hoverIdent(baseId->getText(), hoveredToken, tree,
                              bindings, cursorLine, project);
        }

        auto resolveFieldType = [&](const std::string& ownerType,
                                    const std::string& fieldName) -> std::string {
            if (ownerType.empty()) return "";

            std::string lookupType = ownerType;
            std::unordered_map<std::string, std::string> subst;
            std::string base;
            std::vector<std::string> args;
            if (parseGenericInstance(ownerType, base, args)) {
                lookupType = base;
            }

            if (auto* sd = findStructDecl(tree, lookupType)) {
                if (!args.empty() && sd->typeParamList()) {
                    auto tps = sd->typeParamList()->typeParam();
                    for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
                        auto ids = tps[i]->IDENTIFIER();
                        if (!ids.empty()) subst[ids[0]->getText()] = args[i];
                    }
                }
                for (auto* f : sd->structField()) {
                    if (safeText(f->IDENTIFIER()) == fieldName)
                        return substituteTypeParams(typeSpecToString(f->typeSpec()), subst);
                }
            }
            if (auto* ud = findUnionDecl(tree, lookupType)) {
                if (!args.empty() && ud->typeParamList()) {
                    auto tps = ud->typeParamList()->typeParam();
                    for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
                        auto ids = tps[i]->IDENTIFIER();
                        if (!ids.empty()) subst[ids[0]->getText()] = args[i];
                    }
                }
                for (auto* f : ud->unionField()) {
                    if (safeText(f->IDENTIFIER()) == fieldName)
                        return substituteTypeParams(typeSpecToString(f->typeSpec()), subst);
                }
            }
            if (auto* ti = typeRegistry_.lookup(ownerType)) {
                if (ti->kind == TypeKind::Struct || ti->kind == TypeKind::Union) {
                    for (auto& f : ti->fields) {
                        if (f.name == fieldName && f.typeInfo)
                            return f.typeInfo->name;
                    }
                }
            }
            if (auto* cs = bindings.findStruct(ownerType)) {
                for (auto& f : cs->fields) {
                    if (f.name == fieldName && f.typeInfo)
                        return f.typeInfo->name;
                }
            }
            return "";
        };

        if (fieldId && fieldId->getSymbol() == hoveredToken && baseId) {
            std::string currentType;
            auto* func = findEnclosingFunction(tree, cursorLine);
            if (func) {
                auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
                auto it = locals.find(baseId->getText());
                if (it != locals.end()) currentType = it->second.typeName;
            }

            // In extend instance methods, self is the implicit receiver.
            if (currentType.empty() && baseId->getText() == "self") {
                size_t tokenLine = cursorLine + 1;
                for (auto* tld : tree->topLevelDecl()) {
                    auto* ext = tld->extendDecl();
                    if (!ext) continue;
                    for (auto* method : ext->extendMethod()) {
                        auto* ms = method->getStart();
                        auto* me = method->getStop();
                        if (!ms || !me) continue;
                        if (tokenLine < ms->getLine() || tokenLine > me->getLine()) continue;
                        if (method->AMPERSAND() && method->IDENTIFIER().size() >= 2 &&
                            safeIdAt(method, 1) == "self") {
                            currentType = "*" + safeText(ext->IDENTIFIER());
                            break;
                        }
                    }
                    if (!currentType.empty()) break;
                }
            }

            // Resolve dotted chain before arrow: a.b.c->x
            for (size_t i = 1; i + 1 < ids.size(); i++) {
                auto dotField = ids[i]->getText();
                std::string owner = currentType;
                if (!owner.empty() && owner[0] == '*') owner = owner.substr(1);
                auto nextType = resolveFieldType(owner, dotField);
                if (nextType.empty()) {
                    currentType.clear();
                    break;
                }
                currentType = nextType;
            }

            std::string owner = currentType;
            if (!owner.empty() && owner[0] == '*') owner = owner.substr(1);
            auto finalType = resolveFieldType(owner, fieldId->getText());
            if (!owner.empty() && !finalType.empty()) {
                std::string md = "```lux\n(field) " + finalType + " "
                               + owner + "." + fieldId->getText() + "\n```";
                return makeResult(fieldId->getSymbol(), md);
            }
        }

        // Hover on dotted identifiers before arrow
        if (ids.size() > 2) {
            std::string currentType;
            auto* func = findEnclosingFunction(tree, cursorLine);
            if (func) {
                auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
                auto it = locals.find(baseId->getText());
                if (it != locals.end()) currentType = it->second.typeName;
            }
            for (size_t i = 1; i + 1 < ids.size(); i++) {
                auto* dotId = ids[i];
                std::string owner = currentType;
                if (!owner.empty() && owner[0] == '*') owner = owner.substr(1);
                auto nextType = resolveFieldType(owner, dotId->getText());
                if (dotId && dotId->getSymbol() == hoveredToken && !owner.empty() && !nextType.empty()) {
                    std::string md = "```lux\n(field) " + nextType + " "
                                   + owner + "." + dotId->getText() + "\n```";
                    return makeResult(dotId->getSymbol(), md);
                }
                if (nextType.empty()) break;
                currentType = nextType;
            }
        }

        // Recurse into RHS expression to resolve method/field hover there
        if (auto* rhs = aas->expression()) {
            auto r = walkExprForHover(rhs, hoveredToken, tokenText,
                                      tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    // Check arrow compound assignment statements: ptr->field += expr;
    if (auto* acs = stmt->arrowCompoundAssignStmt()) {
        auto ids = acs->IDENTIFIER();
        if (ids.empty()) return std::nullopt;
        auto* baseId = ids[0];
        auto* fieldId = ids.size() >= 2 ? ids.back() : nullptr;

        if (baseId && baseId->getSymbol() == hoveredToken) {
            return hoverIdent(baseId->getText(), hoveredToken, tree,
                              bindings, cursorLine, project);
        }

        auto resolveFieldType = [&](const std::string& ownerType,
                                    const std::string& fieldName) -> std::string {
            if (ownerType.empty()) return "";

            std::string lookupType = ownerType;
            std::unordered_map<std::string, std::string> subst;
            std::string base;
            std::vector<std::string> args;
            if (parseGenericInstance(ownerType, base, args)) {
                lookupType = base;
            }

            if (auto* sd = findStructDecl(tree, lookupType)) {
                if (!args.empty() && sd->typeParamList()) {
                    auto tps = sd->typeParamList()->typeParam();
                    for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
                        auto ids = tps[i]->IDENTIFIER();
                        if (!ids.empty()) subst[ids[0]->getText()] = args[i];
                    }
                }
                for (auto* f : sd->structField()) {
                    if (safeText(f->IDENTIFIER()) == fieldName)
                        return substituteTypeParams(typeSpecToString(f->typeSpec()), subst);
                }
            }
            if (auto* ud = findUnionDecl(tree, lookupType)) {
                if (!args.empty() && ud->typeParamList()) {
                    auto tps = ud->typeParamList()->typeParam();
                    for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
                        auto ids = tps[i]->IDENTIFIER();
                        if (!ids.empty()) subst[ids[0]->getText()] = args[i];
                    }
                }
                for (auto* f : ud->unionField()) {
                    if (safeText(f->IDENTIFIER()) == fieldName)
                        return substituteTypeParams(typeSpecToString(f->typeSpec()), subst);
                }
            }
            if (auto* ti = typeRegistry_.lookup(ownerType)) {
                if (ti->kind == TypeKind::Struct || ti->kind == TypeKind::Union) {
                    for (auto& f : ti->fields) {
                        if (f.name == fieldName && f.typeInfo)
                            return f.typeInfo->name;
                    }
                }
            }
            if (auto* cs = bindings.findStruct(ownerType)) {
                for (auto& f : cs->fields) {
                    if (f.name == fieldName && f.typeInfo)
                        return f.typeInfo->name;
                }
            }
            return "";
        };

        if (fieldId && fieldId->getSymbol() == hoveredToken && baseId) {
            std::string currentType;
            auto* func = findEnclosingFunction(tree, cursorLine);
            if (func) {
                auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
                auto it = locals.find(baseId->getText());
                if (it != locals.end()) currentType = it->second.typeName;
            }

            if (currentType.empty() && baseId->getText() == "self") {
                size_t tokenLine = cursorLine + 1;
                for (auto* tld : tree->topLevelDecl()) {
                    auto* ext = tld->extendDecl();
                    if (!ext) continue;
                    for (auto* method : ext->extendMethod()) {
                        auto* ms = method->getStart();
                        auto* me = method->getStop();
                        if (!ms || !me) continue;
                        if (tokenLine < ms->getLine() || tokenLine > me->getLine()) continue;
                        if (method->AMPERSAND() && method->IDENTIFIER().size() >= 2 &&
                            safeIdAt(method, 1) == "self") {
                            currentType = "*" + safeText(ext->IDENTIFIER());
                            break;
                        }
                    }
                    if (!currentType.empty()) break;
                }
            }

            for (size_t i = 1; i + 1 < ids.size(); i++) {
                auto dotField = ids[i]->getText();
                std::string owner = currentType;
                if (!owner.empty() && owner[0] == '*') owner = owner.substr(1);
                auto nextType = resolveFieldType(owner, dotField);
                if (nextType.empty()) {
                    currentType.clear();
                    break;
                }
                currentType = nextType;
            }

            std::string owner = currentType;
            if (!owner.empty() && owner[0] == '*') owner = owner.substr(1);
            auto finalType = resolveFieldType(owner, fieldId->getText());
            if (!owner.empty() && !finalType.empty()) {
                std::string md = "```lux\n(field) " + finalType + " "
                               + owner + "." + fieldId->getText() + "\n```";
                return makeResult(fieldId->getSymbol(), md);
            }
        }

        if (ids.size() > 2) {
            std::string currentType;
            auto* func = findEnclosingFunction(tree, cursorLine);
            if (func) {
                auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
                auto it = locals.find(baseId->getText());
                if (it != locals.end()) currentType = it->second.typeName;
            }
            for (size_t i = 1; i + 1 < ids.size(); i++) {
                auto* dotId = ids[i];
                std::string owner = currentType;
                if (!owner.empty() && owner[0] == '*') owner = owner.substr(1);
                auto nextType = resolveFieldType(owner, dotId->getText());
                if (dotId && dotId->getSymbol() == hoveredToken && !owner.empty() && !nextType.empty()) {
                    std::string md = "```lux\n(field) " + nextType + " "
                                   + owner + "." + dotId->getText() + "\n```";
                    return makeResult(dotId->getSymbol(), md);
                }
                if (nextType.empty()) break;
                currentType = nextType;
            }
        }

        if (auto* rhs = acs->expression()) {
            auto r = walkExprForHover(rhs, hoveredToken, tokenText,
                                      tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    // Check call statements (standalone function calls)
    if (auto* cs = stmt->callStmt()) {
        if (auto* al = cs->argList()) {
            for (auto* a : al->expression()) {
                auto r = walkExprForHover(a, hoveredToken, tokenText,
                                           tree, bindings, cursorLine, project);
                if (r) return r;
            }
        }
    }

    // Recurse into nested blocks
    if (auto* ifS = stmt->ifStmt()) {
        if (auto* cond = ifS->expression()) {
            auto r = walkExprForHover(cond, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
        std::optional<HoverResult> r;
        if (auto* body = ifS->ifBody()) {
            if (auto* b = body->block()) {
                r = walkBlockForHover(b, hoveredToken, tokenText,
                                      tree, bindings, cursorLine, project);
            } else if (auto* s = body->statement()) {
                r = walkStmtForHover(s, hoveredToken, tokenText,
                                     tree, bindings, cursorLine, project);
            }
        }
        if (r) return r;
        for (auto* elif : ifS->elseIfClause()) {
            if (auto* ec = elif->expression()) {
                r = walkExprForHover(ec, hoveredToken, tokenText,
                                      tree, bindings, cursorLine, project);
                if (r) return r;
            }
            if (auto* body = elif->ifBody()) {
                if (auto* b = body->block()) {
                    r = walkBlockForHover(b, hoveredToken, tokenText,
                                          tree, bindings, cursorLine, project);
                } else if (auto* s = body->statement()) {
                    r = walkStmtForHover(s, hoveredToken, tokenText,
                                         tree, bindings, cursorLine, project);
                }
            }
            if (r) return r;
        }
        if (auto* el = ifS->elseClause()) {
            if (auto* body = el->ifBody()) {
                if (auto* b = body->block()) {
                    r = walkBlockForHover(b, hoveredToken, tokenText,
                                          tree, bindings, cursorLine, project);
                } else if (auto* s = body->statement()) {
                    r = walkStmtForHover(s, hoveredToken, tokenText,
                                         tree, bindings, cursorLine, project);
                }
            }
            if (r) return r;
        }
    }

    if (auto* fs = stmt->forStmt()) {
        if (auto* fin = dynamic_cast<LuxParser::ForInStmtContext*>(fs)) {
            if (auto* ts = fin->typeSpec()) {
                auto r = hoverTypeSpec(ts, hoveredToken, tree, bindings, project);
                if (r) return r;
            }
            auto r = walkExprForHover(fin->expression(), hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
            r = walkBlockForHover(fin->block(), hoveredToken, tokenText,
                                   tree, bindings, cursorLine, project);
            if (r) return r;
        }
        if (auto* fc = dynamic_cast<LuxParser::ForClassicStmtContext*>(fs)) {
            if (auto* ts = fc->typeSpec()) {
                auto r = hoverTypeSpec(ts, hoveredToken, tree, bindings, project);
                if (r) return r;
            }
            for (auto* e : fc->expression()) {
                auto r = walkExprForHover(e, hoveredToken, tokenText,
                                           tree, bindings, cursorLine, project);
                if (r) return r;
            }
            auto r = walkBlockForHover(fc->block(), hoveredToken, tokenText,
                                        tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    if (auto* ws = stmt->whileStmt()) {
        auto r = walkExprForHover(ws->expression(), hoveredToken, tokenText,
                                   tree, bindings, cursorLine, project);
        if (r) return r;
        r = walkBlockForHover(ws->block(), hoveredToken, tokenText,
                               tree, bindings, cursorLine, project);
        if (r) return r;
    }

    if (auto* dw = stmt->doWhileStmt()) {
        auto r = walkBlockForHover(dw->block(), hoveredToken, tokenText,
                                    tree, bindings, cursorLine, project);
        if (r) return r;
        r = walkExprForHover(dw->expression(), hoveredToken, tokenText,
                              tree, bindings, cursorLine, project);
        if (r) return r;
    }

    if (auto* ls = stmt->loopStmt()) {
        auto r = walkBlockForHover(ls->block(), hoveredToken, tokenText,
                                    tree, bindings, cursorLine, project);
        if (r) return r;
    }

    if (auto* sw = stmt->switchStmt()) {
        auto r = walkExprForHover(sw->expression(), hoveredToken, tokenText,
                                   tree, bindings, cursorLine, project);
        if (r) return r;
        for (auto* c : sw->caseClause()) {
            for (auto* e : c->expression()) {
                r = walkExprForHover(e, hoveredToken, tokenText,
                                      tree, bindings, cursorLine, project);
                if (r) return r;
            }
            r = walkBlockForHover(c->block(), hoveredToken, tokenText,
                                   tree, bindings, cursorLine, project);
            if (r) return r;
        }
        if (auto* dc = sw->defaultClause()) {
            r = walkBlockForHover(dc->block(), hoveredToken, tokenText,
                                   tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    if (auto* tc = stmt->tryCatchStmt()) {
        auto r = walkBlockForHover(tc->block(), hoveredToken, tokenText,
                                    tree, bindings, cursorLine, project);
        if (r) return r;
        for (auto* cc : tc->catchClause()) {
            if (auto* ts = cc->typeSpec()) {
                r = hoverTypeSpec(ts, hoveredToken, tree, bindings, project);
                if (r) return r;
            }
            // Hover on the catch variable name: catch (Error e)
            if (cc->IDENTIFIER() &&
                cc->IDENTIFIER()->getSymbol() == hoveredToken) {
                std::string typeName = cc->typeSpec()
                                        ? safeText(cc->typeSpec()) : "Error";
                std::string varName = safeText(cc->IDENTIFIER());
                std::string md = "```lux\n(catch) " + typeName
                               + " " + varName + "\n```";
                return makeResult(cc->IDENTIFIER()->getSymbol(), md);
            }
            r = walkBlockForHover(cc->block(), hoveredToken, tokenText,
                                   tree, bindings, cursorLine, project);
            if (r) return r;
        }
        if (auto* fc = tc->finallyClause()) {
            r = walkBlockForHover(fc->block(), hoveredToken, tokenText,
                                   tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    // Throw statement — walk into the expression
    if (auto* ts = stmt->throwStmt()) {
        auto r = walkExprForHover(ts->expression(), hoveredToken, tokenText,
                                   tree, bindings, cursorLine, project);
        if (r) return r;
    }

    // Defer statement — walk into the inner call/expression
    if (auto* ds = stmt->deferStmt()) {
        if (auto* cs = ds->callStmt()) {
            // Hover on the function name in defer call
            if (cs->IDENTIFIER() && cs->IDENTIFIER()->getSymbol() == hoveredToken) {
                return hoverIdent(tokenText, hoveredToken, tree,
                                   bindings, cursorLine, project);
            }
            if (auto* al = cs->argList()) {
                for (auto* a : al->expression()) {
                    auto r = walkExprForHover(a, hoveredToken, tokenText,
                                               tree, bindings, cursorLine, project);
                    if (r) return r;
                }
            }
        }
        if (auto* es = ds->exprStmt()) {
            auto r = walkExprForHover(es->expression(), hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    // Naked block { ... } — recurse into body (own scope, no keyword to hover)
    if (auto* nb = stmt->nakedBlockStmt()) {
        for (auto* s : nb->statement()) {
            auto r = walkStmtForHover(s, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    // Inline block #inline { ... } — recurse into body (injects into parent scope)
    if (auto* ib = stmt->inlineBlockStmt()) {
        for (auto* s : ib->statement()) {
            auto r = walkStmtForHover(s, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    // Scope block #scope (callbacks) { ... } — hover callbacks + recurse into body
    if (auto* sb = stmt->scopeBlockStmt()) {
        if (sb->scopeCallbackList()) {
            // Callbacks execute at scope exit and can reference variables declared
            // inside the body. Use the block's end line so collectLocals sees all
            // body locals when resolving identifiers inside callback arguments.
            size_t cbCursorLine = cursorLine;
            if (auto* stop = sb->getStop())
                cbCursorLine = stop->getLine();

            for (auto* cb : sb->scopeCallbackList()->scopeCallback()) {
                if (cb->DOT()) {
                    // dot-access: varName.methodName(args)
                    auto* varTok    = cb->IDENTIFIER(0);
                    auto* methodTok = cb->IDENTIFIER(1);

                    if (varTok && varTok->getSymbol() == hoveredToken) {
                        // Hovering on the receiver variable — normal ident hover
                        return hoverIdent(tokenText, hoveredToken, tree,
                                           bindings, cbCursorLine, project);
                    }

                    if (methodTok && methodTok->getSymbol() == hoveredToken) {
                        // Hovering on the method name — resolve receiver type, search extend blocks
                        std::string varName    = varTok ? varTok->getText() : "";
                        std::string methodName = methodTok->getText();
                        std::string receiverTypeName;

                        if (!varName.empty()) {
                            auto* encFunc = findEnclosingFunction(tree, cbCursorLine);
                            if (encFunc) {
                                auto locals = collectLocals(encFunc, cbCursorLine,
                                                            tree, &bindings, project);
                                auto lit = locals.find(varName);
                                if (lit != locals.end())
                                    receiverTypeName = lit->second.typeName;
                            }
                        }

                        // Search local extend blocks for this method
                        auto tryExtend = [&](LuxParser::ExtendDeclContext* ext,
                                              LuxParser::ExtendMethodContext* m,
                                              const std::vector<DocComment>& docs)
                                -> std::optional<HoverResult> {
                            if (safeIdAt(m, 0) != methodName) return std::nullopt;
                            if (!receiverTypeName.empty() &&
                                safeText(ext->IDENTIFIER()) != receiverTypeName)
                                return std::nullopt;

                            std::ostringstream ss;
                            ss << "```lux\n(" << safeText(ext->IDENTIFIER())
                               << " method) " << typeSpecToString(m->typeSpec())
                               << " " << methodName << "(";
                            if (m->AMPERSAND()) {
                                ss << "&self";
                                for (auto* p : m->param())
                                    ss << ", " << typeSpecToString(p->typeSpec())
                                       << " " << safeText(p->IDENTIFIER());
                            } else if (auto* pl = m->paramList()) {
                                bool first = true;
                                for (auto* p : pl->param()) {
                                    if (!first) ss << ", ";
                                    first = false;
                                    ss << typeSpecToString(p->typeSpec())
                                       << " " << safeText(p->IDENTIFIER());
                                }
                            }
                            ss << ")\n```";
                            std::string md = ss.str();
                            md = appendDocToHover(md, docs, m->getStart()->getLine());
                            return makeResult(methodTok->getSymbol(), md);
                        };

                        for (auto* tld : tree->topLevelDecl()) {
                            auto* ext = tld->extendDecl();
                            if (!ext) continue;
                            for (auto* m : ext->extendMethod()) {
                                if (auto r = tryExtend(ext, m, docComments_))
                                    return r;
                            }
                        }

                        if (project && project->isValid()) {
                            for (auto& ns : project->registry().allNamespaces()) {
                                for (auto* sym : project->registry().getNamespaceSymbols(ns)) {
                                    if (sym->kind != ExportedSymbol::ExtendBlock) continue;
                                    auto* ext = dynamic_cast<LuxParser::ExtendDeclContext*>(sym->decl);
                                    if (!ext) continue;
                                    for (auto* m : ext->extendMethod()) {
                                        if (auto r = tryExtend(ext, m,
                                                docCommentsForFile(sym->sourceFile)))
                                            return r;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    // plain call: funcName(args)
                    if (cb->IDENTIFIER(0) && cb->IDENTIFIER(0)->getSymbol() == hoveredToken) {
                        return hoverIdent(tokenText, hoveredToken, tree,
                                           bindings, cbCursorLine, project);
                    }
                }
                if (auto* al = cb->argList()) {
                    for (auto* a : al->expression()) {
                        auto r = walkExprForHover(a, hoveredToken, tokenText,
                                                   tree, bindings, cbCursorLine, project);
                        if (r) return r;
                    }
                }
            }
        }
        for (auto* s : sb->statement()) {
            auto r = walkStmtForHover(s, hoveredToken, tokenText,
                                       tree, bindings, cursorLine, project);
            if (r) return r;
        }
    }

    return std::nullopt;
}

std::optional<HoverResult> HoverProvider::walkBlockForHover(
        LuxParser::BlockContext* block,
        antlr4::Token* hoveredToken,
        const std::string& tokenText,
        LuxParser::ProgramContext* tree,
        const CBindings& bindings,
        size_t cursorLine,
        const ProjectContext* project) {
    if (!block) return std::nullopt;
    for (auto* stmt : block->statement()) {
        auto r = walkStmtForHover(stmt, hoveredToken, tokenText,
                                   tree, bindings, cursorLine, project);
        if (r) return r;
    }
    return std::nullopt;
}

std::optional<HoverResult> HoverProvider::walkTreeForHover(
        LuxParser::BlockContext* block,
        antlr4::Token* hoveredToken,
        const std::string& tokenText,
        LuxParser::ProgramContext* tree,
        const CBindings& bindings,
        size_t cursorLine,
        const ProjectContext* project) {
    return walkBlockForHover(block, hoveredToken, tokenText,
                              tree, bindings, cursorLine, project);
}

// ═══════════════════════════════════════════════════════════════════════
//  Hover on type specifier
// ═══════════════════════════════════════════════════════════════════════

std::optional<HoverResult> HoverProvider::hoverTypeSpec(
        LuxParser::TypeSpecContext* ts,
        antlr4::Token* hoveredToken,
        LuxParser::ProgramContext* tree,
        const CBindings& bindings,
        const ProjectContext* project) {
    if (!ts || !containsToken(ts, hoveredToken)) return std::nullopt;

    // Check if hovering on an IDENTIFIER in the type spec (user/C type name)
    if (auto* id = ts->IDENTIFIER()) {
        if (id->getSymbol() == hoveredToken) {
            std::string name = id->getText();
            return hoverTypeName(name, hoveredToken, tree, bindings, project);
        }
    }

    // Check if hovering on collection type keywords (tuple, Vec, Map, Set)
    if (ts->TUPLE() && ts->TUPLE()->getSymbol() == hoveredToken)
        return makeResult(hoveredToken, "```lux\n(keyword) tuple\n```\nFixed-size heterogeneous collection type.");
    if (ts->VEC() && ts->VEC()->getSymbol() == hoveredToken)
        return makeResult(hoveredToken, "```lux\n(keyword) Vec\n```\nDynamic array collection type.");
    if (ts->MAP() && ts->MAP()->getSymbol() == hoveredToken)
        return makeResult(hoveredToken, "```lux\n(keyword) Map\n```\nKey-value association collection type.");
    if (ts->SET() && ts->SET()->getSymbol() == hoveredToken)
        return makeResult(hoveredToken, "```lux\n(keyword) Set\n```\nUnique-element collection type.");

    // Check if hovering on a primitive type keyword
    if (auto* pt = ts->primitiveType()) {
        if (containsToken(pt, hoveredToken)) {
            std::string name = pt->getText();
            auto* ti = typeRegistry_.lookup(name);
            if (ti) {
                std::string md = "```lux\n(type) " + ti->name + "\n```";
                return makeResult(hoveredToken, md);
            }
        }
    }

    // Recurse into nested type specs (pointers, arrays, generics)
    for (auto* nested : ts->typeSpec()) {
        auto r = hoverTypeSpec(nested, hoveredToken, tree, bindings, project);
        if (r) return r;
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  Hover on type name (IDENTIFIER in type position)
// ═══════════════════════════════════════════════════════════════════════

std::optional<HoverResult> HoverProvider::hoverTypeName(
        const std::string& name,
        antlr4::Token* token,
        LuxParser::ProgramContext* tree,
        const CBindings& bindings,
        const ProjectContext* project) {

    // User struct
    auto* sd = findStructDecl(tree, name);
    if (sd) return makeResult(token, formatStructDecl(sd));

    // User enum
    auto* ed = findEnumDecl(tree, name);
    if (ed) return makeResult(token, formatEnumDecl(ed));

    // User union
    auto* ud = findUnionDecl(tree, name);
    if (ud) return makeResult(token, formatUnionDecl(ud));

    // Type alias
    auto* ta = findTypeAliasDecl(tree, name);
    if (ta) {
        std::string md = "```lux\ntype " + name + " = "
                       + typeSpecToString(ta->typeSpec()) + "\n```";
        return makeResult(token, md);
    }

    // C struct
    auto* cs = bindings.findStruct(name);
    if (cs) return makeResult(token, formatCStruct(name, *cs));

    // C enum
    auto* ce = bindings.findEnum(name);
    if (ce) return makeResult(token, formatCEnum(name, *ce));

    // C typedef
    auto* ct = bindings.findTypedef(name);
    if (ct) {
        std::string md = "```c\ntypedef " + ct->underlying->name + " " + name + "\n```";
        return makeResult(token, md);
    }

    // Extended type (Vec, Map, Set, Task)
    auto* ext = extTypeRegistry_.lookup(name);
    if (ext) {
        std::ostringstream ss;
        ss << "```lux\n(type) " << name;
        if (ext->genericArity == 1) ss << "<T>";
        else if (ext->genericArity == 2) ss << "<K, V>";
        ss << "\n```\n\n**Methods:**\n```lux\n";
        for (auto& m : ext->methods) {
            ss << m.returnType << " " << m.name << "(";
            for (size_t i = 0; i < m.paramTypes.size(); i++) {
                if (i > 0) ss << ", ";
                ss << m.paramTypes[i];
            }
            ss << ")\n";
        }
        ss << "```";
        return makeResult(token, ss.str());
    }

    // Built-in type (primitives or built-in structs like Error)
    auto* ti = typeRegistry_.lookup(name);
    if (ti) {
        if (ti->kind == TypeKind::Struct && !ti->fields.empty()) {
            std::ostringstream ss;
            ss << "```lux\nstruct " << ti->name << " {\n";
            for (auto& f : ti->fields) {
                ss << "    " << f.typeInfo->name << " " << f.name;
                if (f.autoFill) ss << "  // auto-filled";
                ss << "\n";
            }
            ss << "}\n```";
            return makeResult(token, ss.str());
        }
        std::string md = "```lux\n(type) " + ti->name + "\n```";
        return makeResult(token, md);
    }

    // Cross-file type from project registry
    if (project && project->isValid()) {
        for (auto& ns : project->registry().allNamespaces()) {
            auto* sym = project->registry().findSymbol(ns, name);
            if (!sym) continue;
            if (sym->kind == ExportedSymbol::Struct) {
                if (auto* sdc = dynamic_cast<LuxParser::StructDeclContext*>(sym->decl))
                    return makeResult(token, formatStructDecl(sdc));
            } else if (sym->kind == ExportedSymbol::Enum) {
                if (auto* edc = dynamic_cast<LuxParser::EnumDeclContext*>(sym->decl))
                    return makeResult(token, formatEnumDecl(edc));
            } else if (sym->kind == ExportedSymbol::Union) {
                if (auto* udc = dynamic_cast<LuxParser::UnionDeclContext*>(sym->decl))
                    return makeResult(token, formatUnionDecl(udc));
            } else if (sym->kind == ExportedSymbol::TypeAlias) {
                if (auto* tac = dynamic_cast<LuxParser::TypeAliasDeclContext*>(sym->decl)) {
                    std::string md = "```lux\ntype " + name + " = "
                                   + typeSpecToString(tac->typeSpec()) + "\n```";
                    return makeResult(token, md);
                }
            }
        }
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  Hover on method call: expr.method(args)
// ═══════════════════════════════════════════════════════════════════════

std::optional<HoverResult> HoverProvider::hoverMethodCall(
        LuxParser::MethodCallExprContext* ctx,
        LuxParser::ProgramContext* tree,
        const CBindings& bindings,
        size_t cursorLine,
        const ProjectContext* project) {
    std::string methodName = safeText(ctx->IDENTIFIER());
    auto* receiver = ctx->expression();

    // ── Step 1: Resolve the receiver's type FIRST (like the Checker does) ──
    std::string receiverTypeName;
    std::string receiverText;

    if (auto* ident = dynamic_cast<LuxParser::IdentExprContext*>(receiver)) {
        receiverText = safeText(ident->IDENTIFIER());
    }

    auto* encFunc = findEnclosingFunction(tree, cursorLine);
    if (encFunc) {
        auto locals = collectLocals(encFunc, cursorLine, tree, &bindings, project);

        FuncLookupCtx flc;
        flc.tree = tree;
        flc.bindings = &bindings;
        flc.builtinReg = &builtinRegistry_;
        flc.intrinsicReg = &intrinsicRegistry_;
        flc.extTypeReg = &extTypeRegistry_;
        flc.methodReg = &methodRegistry_;
        flc.project = project;

        if (!receiverText.empty()) {
            auto lit = locals.find(receiverText);
            if (lit != locals.end())
                receiverTypeName = lit->second.typeName;
        }

        if (receiverTypeName.empty()) {
            receiverTypeName = inferExprTypeName(receiver, locals, &flc);
        }
    }

    // ── Step 2: Determine receiver category ──
    // Check if this is an extended/collection type (vec<T>, map<K,V>, set<T>)
    // Strip pointer indirection (e.g. *vec<T> → vec<T>)
    std::string strippedReceiver = receiverTypeName;
    while (!strippedReceiver.empty() && strippedReceiver[0] == '*')
        strippedReceiver = strippedReceiver.substr(1);

    std::string extBaseName;
    if (!strippedReceiver.empty()) {
        auto ltPos = strippedReceiver.find('<');
        if (ltPos != std::string::npos) {
            std::string raw = strippedReceiver.substr(0, ltPos);
            std::string normalized = normalizeExtBaseName(raw);
            if (normalized == "Vec" || normalized == "Map" ||
                normalized == "Set" || normalized == "Task")
                extBaseName = normalized;
        }
    }

    // Check if receiver is an array type (e.g. [10]int32)
    bool isArrayReceiver = !receiverTypeName.empty() &&
                           receiverTypeName.size() > 1 &&
                           receiverTypeName[0] == '[';

    // Determine if receiver is a known primitive type
    TypeKind receiverKind = TypeKind::Void;
    bool hasPrimitiveKind = false;
    if (!receiverTypeName.empty() && extBaseName.empty() && !isArrayReceiver) {
        // Map type name to TypeKind
        auto* typeInfo = typeRegistry_.lookup(receiverTypeName);
        if (typeInfo) {
            receiverKind = typeInfo->kind;
            hasPrimitiveKind = true;
        }
    }

    // ── Step 3: Dispatch to the correct registry based on receiver type ──

    // 3a) Extended/collection type methods (vec.push, map.get, etc.)
    if (!extBaseName.empty()) {
        // Parse generic type args for placeholder substitution
        auto ltPos = strippedReceiver.find('<');
        auto gtPos = strippedReceiver.rfind('>');
        std::vector<std::string> typeArgs;
        if (ltPos != std::string::npos && gtPos != std::string::npos) {
            std::string inner = strippedReceiver.substr(ltPos + 1, gtPos - ltPos - 1);
            int depth = 0;
            size_t start = 0;
            for (size_t i = 0; i < inner.size(); i++) {
                if (inner[i] == '<') depth++;
                else if (inner[i] == '>') depth--;
                else if (inner[i] == ',' && depth == 0) {
                    auto arg = inner.substr(start, i - start);
                    size_t b = arg.find_first_not_of(' ');
                    size_t e = arg.find_last_not_of(' ');
                    if (b != std::string::npos)
                        typeArgs.push_back(arg.substr(b, e - b + 1));
                    start = i + 1;
                }
            }
            auto arg = inner.substr(start);
            size_t b = arg.find_first_not_of(' ');
            size_t e = arg.find_last_not_of(' ');
            if (b != std::string::npos)
                typeArgs.push_back(arg.substr(b, e - b + 1));
        }

        auto substituteGeneric = [&](const std::string& placeholder) -> std::string {
            if (placeholder.empty() || placeholder[0] != '_') return placeholder;
            if (placeholder == "_self")  return strippedReceiver;
            if (placeholder == "_elem" && !typeArgs.empty()) return typeArgs[0];
            if (placeholder == "_key"  && !typeArgs.empty()) return typeArgs[0];
            if (placeholder == "_val"  && typeArgs.size() >= 2) return typeArgs[1];
            if (placeholder == "_val"  && typeArgs.size() == 1) return typeArgs[0];
            if (placeholder == "_vec_key" && !typeArgs.empty())
                return "vec<" + typeArgs[0] + ">";
            if (placeholder == "_vec_val" && typeArgs.size() >= 2)
                return "vec<" + typeArgs[1] + ">";
            if (placeholder == "_vec_val" && typeArgs.size() == 1)
                return "vec<" + typeArgs[0] + ">";
            return placeholder;
        };

        auto* desc = extTypeRegistry_.lookup(extBaseName);
        if (desc) {
            // Lowercase display name for native keywords
            std::string displayName = extBaseName;
            if (displayName == "Vec") displayName = "vec";
            else if (displayName == "Map") displayName = "map";
            else if (displayName == "Set") displayName = "set";

            for (auto& m : desc->methods) {
                if (m.name == methodName) {
                    std::ostringstream ss;
                    std::string retType = substituteGeneric(m.returnType);
                    ss << "```lux\n(" << displayName << " method) " << retType
                       << " " << methodName << "(";
                    for (size_t i = 0; i < m.paramTypes.size(); i++) {
                        if (i > 0) ss << ", ";
                        ss << substituteGeneric(m.paramTypes[i]);
                    }
                    ss << ")\n```";
                    return makeResult(ctx->IDENTIFIER()->getSymbol(), ss.str());
                }
            }
        }
    }

    // 3b) Array methods (e.g. [10]int32.len)
    if (isArrayReceiver) {
        auto* arrayMd = methodRegistry_.lookupArrayMethod(methodName);
        if (arrayMd) {
            std::ostringstream ss;
            ss << "```lux\n(method) " << arrayMd->returnType << " " << methodName << "(";
            for (size_t i = 0; i < arrayMd->paramTypes.size(); i++) {
                if (i > 0) ss << ", ";
                ss << arrayMd->paramTypes[i];
            }
            ss << ")\n```";
            return makeResult(ctx->IDENTIFIER()->getSymbol(), ss.str());
        }
    }

    // 3c) Primitive type methods — only check the ACTUAL receiver kind
    if (hasPrimitiveKind) {
        auto* md = methodRegistry_.lookup(receiverKind, methodName);
        if (md) {
            std::string retType = md->returnType;
            if (retType == "_self" && !receiverTypeName.empty())
                retType = receiverTypeName;
            std::ostringstream ss;
            ss << "```lux\n(method) " << retType << " " << methodName << "(";
            for (size_t i = 0; i < md->paramTypes.size(); i++) {
                if (i > 0) ss << ", ";
                ss << md->paramTypes[i];
            }
            ss << ")\n```";
            return makeResult(ctx->IDENTIFIER()->getSymbol(), ss.str());
        }
    }

    // 3d) Fallback: if receiver type is unknown, try all registries
    if (receiverTypeName.empty()) {
        // Try extended types (without placeholder resolution)
        for (auto& extName : {"Vec", "Map", "Set", "Task"}) {
            auto* desc = extTypeRegistry_.lookup(extName);
            if (!desc) continue;
            std::string displayName = desc->baseName;
            if (displayName == "Vec") displayName = "vec";
            else if (displayName == "Map") displayName = "map";
            else if (displayName == "Set") displayName = "set";
            for (auto& m : desc->methods) {
                if (m.name == methodName) {
                    std::ostringstream ss;
                    ss << "```lux\n(" << displayName << " method) " << m.returnType
                       << " " << methodName << "(";
                    for (size_t i = 0; i < m.paramTypes.size(); i++) {
                        if (i > 0) ss << ", ";
                        ss << m.paramTypes[i];
                    }
                    ss << ")\n```";
                    return makeResult(ctx->IDENTIFIER()->getSymbol(), ss.str());
                }
            }
        }

        // Try all primitive kinds
        for (auto kind : {TypeKind::Integer, TypeKind::Float, TypeKind::String,
                          TypeKind::Bool, TypeKind::Char}) {
            auto* md = methodRegistry_.lookup(kind, methodName);
            if (md) {
                std::ostringstream ss;
                ss << "```lux\n(method) " << md->returnType << " " << methodName << "(";
                for (size_t i = 0; i < md->paramTypes.size(); i++) {
                    if (i > 0) ss << ", ";
                    ss << md->paramTypes[i];
                }
                ss << ")\n```";
                return makeResult(ctx->IDENTIFIER()->getSymbol(), ss.str());
            }
        }

        // Try array methods
        auto* arrayMd = methodRegistry_.lookupArrayMethod(methodName);
        if (arrayMd) {
            std::ostringstream ss;
            ss << "```lux\n(method) " << arrayMd->returnType << " " << methodName << "(";
            for (size_t i = 0; i < arrayMd->paramTypes.size(); i++) {
                if (i > 0) ss << ", ";
                ss << arrayMd->paramTypes[i];
            }
            ss << ")\n```";
            return makeResult(ctx->IDENTIFIER()->getSymbol(), ss.str());
        }
    }

    // 4) Check user extend block methods (local + cross-file)
    auto formatExtendMethod = [&](LuxParser::ExtendDeclContext* ext,
                                   LuxParser::ExtendMethodContext* m,
                                   const std::vector<DocComment>& docs) -> std::optional<HoverResult> {
        std::ostringstream ss;
        ss << "```lux\n(" << safeText(ext->IDENTIFIER())
           << " method) " << typeSpecToString(m->typeSpec())
           << " " << methodName << "(";
        if (m->AMPERSAND()) {
            ss << "&self";
            for (auto* p : m->param()) {
                ss << ", " << typeSpecToString(p->typeSpec())
                   << " " << safeText(p->IDENTIFIER());
            }
        } else {
            if (auto* params = m->paramList()) {
                bool first = true;
                for (auto* p : params->param()) {
                    if (!first) ss << ", ";
                    first = false;
                    ss << typeSpecToString(p->typeSpec())
                       << " " << safeText(p->IDENTIFIER());
                }
            }
        }
        ss << ")\n```";
        std::string md = ss.str();
        size_t declLine = m->getStart()->getLine();
        md = appendDocToHover(md, docs, declLine);
        return makeResult(ctx->IDENTIFIER()->getSymbol(), md);
    };

    // Local file extend blocks
    for (auto* tld : tree->topLevelDecl()) {
        auto* ext = tld->extendDecl();
        if (!ext) continue;
        for (auto* m : ext->extendMethod()) {
            if (safeIdAt(m, 0) == methodName)
                return formatExtendMethod(ext, m, docComments_);
        }
    }

    // Cross-file extend blocks from project registry
    if (project && project->isValid()) {
        for (auto& ns : project->registry().allNamespaces()) {
            auto syms = project->registry().getNamespaceSymbols(ns);
            for (auto* sym : syms) {
                if (sym->kind != ExportedSymbol::ExtendBlock) continue;
                auto* ext = dynamic_cast<LuxParser::ExtendDeclContext*>(sym->decl);
                if (!ext) continue;
                for (auto* m : ext->extendMethod()) {
                    if (safeIdAt(m, 0) == methodName) {
                        return formatExtendMethod(ext, m,
                            docCommentsForFile(sym->sourceFile));
                    }
                }
            }
        }
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  Hover on field access: expr.field
// ═══════════════════════════════════════════════════════════════════════

std::optional<HoverResult> HoverProvider::hoverFieldAccess(
        LuxParser::FieldAccessExprContext* ctx,
        LuxParser::ProgramContext* tree,
        const CBindings& bindings,
        size_t cursorLine,
        const ProjectContext* project) {
    std::string fieldName = safeText(ctx->IDENTIFIER());
    auto* receiver = ctx->expression();
    auto* token = ctx->IDENTIFIER()->getSymbol();

    // Try to find the receiver's type name
    std::string receiverTypeName;
    auto* func = findEnclosingFunction(tree, cursorLine);
    if (func) {
        auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
        FuncLookupCtx flc;
        flc.tree = tree;
        flc.bindings = &bindings;
        flc.builtinReg = &builtinRegistry_;
        flc.intrinsicReg = &intrinsicRegistry_;
        flc.extTypeReg = &extTypeRegistry_;
        flc.methodReg = &methodRegistry_;
        flc.project = project;
        receiverTypeName = inferExprTypeName(receiver, locals, &flc);
    }

    // Auto-dereference pointer for field access (e.g. n2.next.value)
    std::string derefedType = receiverTypeName;
    while (!derefedType.empty() && derefedType[0] == '*')
        derefedType = derefedType.substr(1);

    std::string lookupReceiverType = derefedType;
    std::unordered_map<std::string, std::string> subst;
    std::string receiverBase;
    std::vector<std::string> receiverArgs;
    if (parseGenericInstance(derefedType, receiverBase, receiverArgs)) {
        lookupReceiverType = receiverBase;
    }

    // Search user structs for this field
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* sd = tld->structDecl()) {
            std::string sName = safeText(sd->IDENTIFIER());
            if (!lookupReceiverType.empty() && sName != lookupReceiverType)
                continue;

            subst.clear();
            if (!receiverArgs.empty() && sd->typeParamList()) {
                auto tps = sd->typeParamList()->typeParam();
                for (size_t i = 0; i < std::min(receiverArgs.size(), tps.size()); i++) {
                    auto ids = tps[i]->IDENTIFIER();
                    if (!ids.empty()) subst[ids[0]->getText()] = receiverArgs[i];
                }
            }
            for (auto* f : sd->structField()) {
                if (safeText(f->IDENTIFIER()) == fieldName) {
                    std::string md = "```lux\n(field) "
                                   + substituteTypeParams(typeSpecToString(f->typeSpec()), subst)
                                   + " " + sName + "." + fieldName + "\n```";
                    return makeResult(token, md);
                }
            }
        }
    }

    // Search user unions for this field
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* ud = tld->unionDecl()) {
            std::string uName = safeText(ud->IDENTIFIER());
            if (!lookupReceiverType.empty() && uName != lookupReceiverType)
                continue;

            subst.clear();
            if (!receiverArgs.empty() && ud->typeParamList()) {
                auto tps = ud->typeParamList()->typeParam();
                for (size_t i = 0; i < std::min(receiverArgs.size(), tps.size()); i++) {
                    auto ids = tps[i]->IDENTIFIER();
                    if (!ids.empty()) subst[ids[0]->getText()] = receiverArgs[i];
                }
            }
            for (auto* f : ud->unionField()) {
                if (safeText(f->IDENTIFIER()) == fieldName) {
                    std::string md = "```lux\n(field) "
                                   + substituteTypeParams(typeSpecToString(f->typeSpec()), subst)
                                   + " " + uName + "." + fieldName + "\n```";
                    return makeResult(token, md);
                }
            }
        }
    }

    // Search built-in structs from TypeRegistry (e.g. Error)
    if (!receiverTypeName.empty()) {
        auto* ti = typeRegistry_.lookup(receiverTypeName);
        if (ti && (ti->kind == TypeKind::Struct || ti->kind == TypeKind::Union)) {
            for (auto& f : ti->fields) {
                if (f.name == fieldName) {
                    std::string md = "```lux\n(field) " + f.typeInfo->name
                                   + " " + receiverTypeName + "." + fieldName + "\n```";
                    return makeResult(token, md);
                }
            }
        }
    } else {
        // Unknown receiver type fallback: try built-in Error fields.
        auto* err = typeRegistry_.lookup("Error");
        if (err && (err->kind == TypeKind::Struct || err->kind == TypeKind::Union)) {
            for (const auto& f : err->fields) {
                if (f.name == fieldName && f.typeInfo) {
                    std::string md = "```lux\n(field) " + f.typeInfo->name
                                   + " Error." + fieldName + "\n```";
                    return makeResult(token, md);
                }
            }
        }
    }

    // Search C structs for this field
    if (!receiverTypeName.empty()) {
        auto* cs = bindings.findStruct(receiverTypeName);
        if (cs) {
            for (auto& f : cs->fields) {
                if (f.name == fieldName) {
                    std::string md = "```c\n(field) " + f.typeInfo->name + " "
                                   + receiverTypeName + "." + fieldName + "\n```\n*C struct field*";
                    return makeResult(token, md);
                }
            }
        }
    } else {
        // No receiver type known — search all C structs
        for (auto& [sName, cs] : bindings.structs()) {
            for (auto& f : cs.fields) {
                if (f.name == fieldName) {
                    std::string md = "```c\n(field) " + f.typeInfo->name + " "
                                   + sName + "." + fieldName + "\n```\n*C struct field*";
                    return makeResult(token, md);
                }
            }
        }
    }

    // Search project registry for cross-file struct fields
    if (project && project->isValid()) {
        for (auto& ns : project->registry().allNamespaces()) {
            auto syms = project->registry().getNamespaceSymbols(ns);
            for (auto* sym : syms) {
                if (sym->kind == ExportedSymbol::Struct) {
                    auto* sd = dynamic_cast<LuxParser::StructDeclContext*>(sym->decl);
                    if (!sd) continue;
                    if (!lookupReceiverType.empty() &&
                        safeText(sd->IDENTIFIER()) != lookupReceiverType)
                        continue;

                    subst.clear();
                    if (!receiverArgs.empty() && sd->typeParamList()) {
                        auto tps = sd->typeParamList()->typeParam();
                        for (size_t i = 0; i < std::min(receiverArgs.size(), tps.size()); i++) {
                            auto ids = tps[i]->IDENTIFIER();
                            if (!ids.empty()) subst[ids[0]->getText()] = receiverArgs[i];
                        }
                    }
                    for (auto* f : sd->structField()) {
                        if (safeText(f->IDENTIFIER()) == fieldName) {
                            std::string md = "```lux\n(field) "
                                           + substituteTypeParams(typeSpecToString(f->typeSpec()), subst)
                                           + " " + safeText(sd->IDENTIFIER()) + "." + fieldName + "\n```";
                            return makeResult(token, md);
                        }
                    }
                }

                if (sym->kind == ExportedSymbol::Union) {
                    auto* ud = dynamic_cast<LuxParser::UnionDeclContext*>(sym->decl);
                    if (!ud) continue;
                    if (!lookupReceiverType.empty() &&
                        safeText(ud->IDENTIFIER()) != lookupReceiverType)
                        continue;

                    subst.clear();
                    if (!receiverArgs.empty() && ud->typeParamList()) {
                        auto tps = ud->typeParamList()->typeParam();
                        for (size_t i = 0; i < std::min(receiverArgs.size(), tps.size()); i++) {
                            auto ids = tps[i]->IDENTIFIER();
                            if (!ids.empty()) subst[ids[0]->getText()] = receiverArgs[i];
                        }
                    }
                    for (auto* f : ud->unionField()) {
                        if (safeText(f->IDENTIFIER()) == fieldName) {
                            std::string md = "```lux\n(field) "
                                           + substituteTypeParams(typeSpecToString(f->typeSpec()), subst)
                                           + " " + safeText(ud->IDENTIFIER()) + "." + fieldName + "\n```";
                            return makeResult(token, md);
                        }
                    }
                }
            }
        }
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  Hover on enum access: Type::Variant
// ═══════════════════════════════════════════════════════════════════════

std::optional<HoverResult> HoverProvider::hoverEnumAccess(
        LuxParser::EnumAccessExprContext* ctx,
        LuxParser::ProgramContext* tree,
        const CBindings& bindings,
        const ProjectContext* project) {
    auto ids = ctx->IDENTIFIER();
    if (ids.size() < 2) return std::nullopt;

    std::string typeName    = ids[0]->getText();
    std::string variantName = ids[1]->getText();
    auto* token = ids[1]->getSymbol();

    // User enum (local file)
    auto* ed = findEnumDecl(tree, typeName);
    if (ed) {
        std::string md = "```lux\n(variant) " + typeName + "::" + variantName + "\n```";
        size_t declLine = ed->getStart()->getLine();
        md = appendDocToHover(md, docComments_, declLine);
        return makeResult(token, md);
    }

    // C enum
    auto* ce = bindings.findEnum(typeName);
    if (ce) {
        for (auto& [vname, val] : ce->values) {
            if (vname == variantName) {
                std::string md = "```c\n(variant) " + typeName + "::" + variantName
                               + " = " + std::to_string(val) + "\n```\n*C enum variant*";
                return makeResult(token, md);
            }
        }
    }

    // Cross-file enum from project registry
    if (project && project->isValid()) {
        for (auto& ns : project->registry().allNamespaces()) {
            auto* sym = project->registry().findSymbol(ns, typeName);
            if (!sym || sym->kind != ExportedSymbol::Enum) continue;
            auto* enumCtx = dynamic_cast<LuxParser::EnumDeclContext*>(sym->decl);
            if (!enumCtx) continue;
            for (auto* variant : enumCtx->enumVariant()) {
                auto* v = variant->IDENTIFIER();
                if (v && v->getText() == variantName) {
                    std::string md = "```lux\n(variant) " + typeName + "::" + variantName + "\n```";
                    // Read doc-comments from source file
                    if (!sym->sourceFile.empty()) {
                        size_t declLine = enumCtx->getStart()->getLine();
                        md = appendDocToHover(md,
                            docCommentsForFile(sym->sourceFile),
                            declLine);
                    }
                    return makeResult(token, md);
                }
            }
        }
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  Hover on static method call: Type::method(args)
// ═══════════════════════════════════════════════════════════════════════

std::optional<HoverResult> HoverProvider::hoverStaticMethodCall(
        LuxParser::StaticMethodCallExprContext* ctx,
        LuxParser::ProgramContext* tree,
        const CBindings& bindings,
        size_t cursorLine,
        const ProjectContext* project) {
    auto ids = ctx->IDENTIFIER();
    if (ids.size() < 2) return std::nullopt;

    std::string ownerPath;
    for (size_t i = 0; i + 1 < ids.size(); ++i) {
        if (!ownerPath.empty()) ownerPath += "::";
        ownerPath += ids[i]->getText();
    }

    std::string typeName = ownerPath;
    auto lastScope = typeName.rfind("::");
    if (lastScope != std::string::npos)
        typeName = typeName.substr(lastScope + 2);

    std::string methodName = ids.back()->getText();
    auto* token = ids.back()->getSymbol();

    // Intrinsic calls, e.g. lux::core::trap(...)
    if (IntrinsicRegistry::isIntrinsicPrefix(ids.front()->getText())) {
        std::string ns, funcName;
        std::vector<std::string> idTexts;
        for (auto* id : ids) idTexts.push_back(id->getText());
        if (IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName)) {
            if (const auto* intrinsic = intrinsicRegistry_.lookup(ns, funcName)) {
                std::ostringstream ss;
                ss << "```lux\n(intrinsic) " << intrinsic->returnType << " "
                   << "lux::" << ns << "::" << funcName << "(";
                for (size_t i = 0; i < intrinsic->params.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << intrinsic->params[i].type;
                }
                if (intrinsic->isVariadic) {
                    if (!intrinsic->params.empty()) ss << ", ";
                    ss << "...";
                }
                ss << ")\n```\n\n" << intrinsic->description;
                return makeResult(token, ss.str());
            }
        }
        return std::nullopt;
    }

    // std module calls, e.g. std::log::println(...)
    if (NamespaceRegistry::isStdModule(ownerPath)) {
        if (const auto* sig = builtinRegistry_.lookup(methodName)) {
            std::ostringstream ss;
            ss << "```lux\n(function) " << sig->returnType << " " << ownerPath
               << "::" << methodName << "(";
            for (size_t i = 0; i < sig->paramTypes.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << sig->paramTypes[i];
            }
            if (sig->isVariadic) {
                if (!sig->paramTypes.empty()) ss << ", ";
                ss << "...";
            }
            ss << ")\n```";
            return makeResult(token, ss.str());
        }
        return std::nullopt;
    }

    // Check user extend block static methods
    for (auto* tld : tree->topLevelDecl()) {
        auto* ext = tld->extendDecl();
        if (!ext) continue;
        if (safeText(ext->IDENTIFIER()) != typeName) continue;
        for (auto* m : ext->extendMethod()) {
            if (safeIdAt(m, 0) == methodName) {
                std::ostringstream ss;
                ss << "```lux\n(static method) " << typeSpecToString(m->typeSpec())
                   << " " << typeName << "::" << methodName << "(";
                if (auto* params = m->paramList()) {
                    bool first = true;
                    for (auto* p : params->param()) {
                        if (!first) ss << ", ";
                        first = false;
                        ss << typeSpecToString(p->typeSpec())
                           << " " << safeText(p->IDENTIFIER());
                    }
                }
                ss << ")\n```";
                std::string md = ss.str();
                size_t declLine = m->getStart()->getLine();
                md = appendDocToHover(md, docComments_, declLine);
                return makeResult(token, md);
            }
        }
    }

    // Check extended type methods (Vec::new, Map::new, etc.)
    auto* extDesc = extTypeRegistry_.lookup(typeName);
    if (extDesc) {
        for (auto& m : extDesc->methods) {
            if (m.name == methodName) {
                std::ostringstream ss;
                ss << "```lux\n(static method) " << m.returnType << " "
                   << typeName << "::" << methodName << "(";
                for (size_t i = 0; i < m.paramTypes.size(); i++) {
                    if (i > 0) ss << ", ";
                    ss << m.paramTypes[i];
                }
                ss << ")\n```";
                return makeResult(token, ss.str());
            }
        }
    }

    // Cross-file extend blocks
    if (project && project->isValid()) {
        for (auto& ns : project->registry().allNamespaces()) {
            auto syms = project->registry().getNamespaceSymbols(ns);
            for (auto* sym : syms) {
                if (sym->kind != ExportedSymbol::ExtendBlock) continue;
                auto* ext = dynamic_cast<LuxParser::ExtendDeclContext*>(sym->decl);
                if (!ext || safeText(ext->IDENTIFIER()) != typeName) continue;
                for (auto* m : ext->extendMethod()) {
                    if (safeIdAt(m, 0) == methodName) {
                        std::ostringstream ss;
                        ss << "```lux\n(static method) " << typeSpecToString(m->typeSpec())
                           << " " << typeName << "::" << methodName << "(";
                        if (auto* params = m->paramList()) {
                            bool first = true;
                            for (auto* p : params->param()) {
                                if (!first) ss << ", ";
                                first = false;
                                ss << typeSpecToString(p->typeSpec())
                                   << " " << safeText(p->IDENTIFIER());
                            }
                        }
                        ss << ")\n```";
                        std::string md = ss.str();
                        // Read doc-comments from the source file
                        if (!sym->sourceFile.empty()) {
                            size_t declLine = m->getStart()->getLine();
                            md = appendDocToHover(md,
                                docCommentsForFile(sym->sourceFile),
                                declLine);
                        }
                        return makeResult(token, md);
                    }
                }
            }
        }
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  Local variable collection
// ═══════════════════════════════════════════════════════════════════════

// (FuncLookupCtx defined near top of file)

// Resolve a type name to its tuple<> form, resolving aliases if needed.
// Returns "" if not a tuple type.
static std::string resolveTupleTypeName(
        const std::string& typeName,
        const FuncLookupCtx* flc) {
    if (typeName.rfind("tuple<", 0) == 0) return typeName;
    // Try alias resolution
    if (flc && flc->tree) {
        for (auto* tld : flc->tree->topLevelDecl()) {
            if (auto* ta = tld->typeAliasDecl()) {
                if (safeText(ta->IDENTIFIER()) == typeName) {
                    auto resolved = safeText(ta->typeSpec());
                    if (resolved.rfind("tuple<", 0) == 0) return resolved;
                }
            }
        }
    }
    return "";
}

// Parse "tuple<T1,T2,...>" into a vector of element type names.
static std::vector<std::string> parseTupleElements(const std::string& tupleType) {
    std::vector<std::string> elems;
    // Strip "tuple<" prefix and ">" suffix
    if (tupleType.size() < 8 || tupleType.rfind("tuple<", 0) != 0) return elems;
    auto inner = tupleType.substr(6, tupleType.size() - 7); // remove "tuple<" and ">"
    // Split by comma, respecting nested <> (for tuple<Vec<int32>, string>)
    int depth = 0;
    size_t start = 0;
    for (size_t i = 0; i < inner.size(); i++) {
        if (inner[i] == '<') depth++;
        else if (inner[i] == '>') depth--;
        else if (inner[i] == ',' && depth == 0) {
            elems.push_back(inner.substr(start, i - start));
            start = i + 1;
        }
    }
    elems.push_back(inner.substr(start));
    return elems;
}

static std::string trimCopy(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\n\r");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\n\r");
    return s.substr(b, e - b + 1);
}

static std::vector<std::string> splitTopLevelComma(const std::string& s) {
    std::vector<std::string> out;
    int depthAngle = 0;
    int depthBracket = 0;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '<') depthAngle++;
        else if (c == '>') depthAngle--;
        else if (c == '[') depthBracket++;
        else if (c == ']') depthBracket--;
        else if (c == ',' && depthAngle == 0 && depthBracket == 0) {
            out.push_back(trimCopy(s.substr(start, i - start)));
            start = i + 1;
        }
    }
    out.push_back(trimCopy(s.substr(start)));
    return out;
}

static bool parseGenericInstance(const std::string& t,
                                 std::string& base,
                                 std::vector<std::string>& args) {
    auto lt = t.find('<');
    auto gt = t.rfind('>');
    if (lt == std::string::npos || gt == std::string::npos || gt <= lt)
        return false;
    base = trimCopy(t.substr(0, lt));
    args = splitTopLevelComma(t.substr(lt + 1, gt - lt - 1));
    return !base.empty();
}

static std::string substituteTypeParams(
    const std::string& type,
    const std::unordered_map<std::string, std::string>& subst) {
    std::string t = trimCopy(type);
    auto it = subst.find(t);
    if (it != subst.end()) return it->second;

    if (t.size() > 1 && t[0] == '*')
        return "*" + substituteTypeParams(t.substr(1), subst);

    if (!t.empty() && t[0] == '[') {
        auto rb = t.find(']');
        if (rb != std::string::npos && rb + 1 < t.size())
            return t.substr(0, rb + 1) + substituteTypeParams(t.substr(rb + 1), subst);
    }

    std::string base;
    std::vector<std::string> args;
    if (parseGenericInstance(t, base, args)) {
        std::string out = base + "<";
        for (size_t i = 0; i < args.size(); i++) {
            if (i) out += ", ";
            out += substituteTypeParams(args[i], subst);
        }
        out += ">";
        return out;
    }

    return t;
}

static bool unifyGenericType(
    const std::string& formalRaw,
    const std::string& actualRaw,
    const std::unordered_set<std::string>& typeParams,
    std::unordered_map<std::string, std::string>& inferred) {
    std::string formal = trimCopy(formalRaw);
    std::string actual = trimCopy(actualRaw);
    if (formal.empty() || actual.empty()) return false;

    if (typeParams.count(formal)) {
        auto it = inferred.find(formal);
        if (it == inferred.end()) {
            inferred[formal] = actual;
            return true;
        }
        return it->second == actual;
    }

    if (formal.size() > 1 && formal[0] == '*') {
        if (actual.size() <= 1 || actual[0] != '*') return false;
        return unifyGenericType(formal.substr(1), actual.substr(1), typeParams, inferred);
    }

    if (!formal.empty() && formal[0] == '[') {
        auto frb = formal.find(']');
        auto arb = actual.find(']');
        if (frb == std::string::npos || arb == std::string::npos) return false;
        return unifyGenericType(formal.substr(frb + 1), actual.substr(arb + 1), typeParams, inferred);
    }

    std::string fBase, aBase;
    std::vector<std::string> fArgs, aArgs;
    bool fGen = parseGenericInstance(formal, fBase, fArgs);
    bool aGen = parseGenericInstance(actual, aBase, aArgs);
    if (fGen || aGen) {
        if (!fGen || !aGen) return false;
        if (normalizeExtBaseName(fBase) != normalizeExtBaseName(aBase)) return false;
        if (fArgs.size() != aArgs.size()) return false;
        for (size_t i = 0; i < fArgs.size(); i++) {
            if (!unifyGenericType(fArgs[i], aArgs[i], typeParams, inferred))
                return false;
        }
        return true;
    }

    return formal == actual;
}

// Look up the return type of a function by name.
static std::string lookupFuncReturnType(
        const std::string& funcName,
        const FuncLookupCtx* flc) {
    if (!flc) return "";

    static const std::unordered_map<std::string, std::string> globalBuiltinReturns = {
        {"typeof", "string"},
        {"toString", "string"},
        {"fromCStr", "string"},
        {"fromCStrCopy", "string"},
        {"fromCStrLen", "string"},
        {"sprintf", "string"},
        {"sizeof", "int64"},
        {"toInt", "int64"},
        {"toFloat", "float64"},
        {"toBool", "bool"}
    };
    if (auto it = globalBuiltinReturns.find(funcName); it != globalBuiltinReturns.end())
        return it->second;

    // 1) C functions from CBindings (most authoritative for #include)
    if (flc->bindings) {
        auto* cfunc = flc->bindings->findFunction(funcName);
        if (cfunc && cfunc->returnType)
            return cfunc->returnType->name;
    }

    // 2) User-defined functions and extern declarations in the AST
    if (flc->tree) {
        for (auto* tld : flc->tree->topLevelDecl()) {
            if (auto* fd = tld->functionDecl()) {
                if (safeIdAt(fd, 0) == funcName)
                    return safeText(fd->typeSpec());
            }
            if (auto* ext = tld->externDecl()) {
                if (safeText(ext->IDENTIFIER()) == funcName) {
                    auto ret = safeText(ext->typeSpec());
                    if (ret != "auto") return ret;
                }
            }
        }
    }

    // 3) Cross-file functions (imported via `use`)
    if (flc->project && flc->project->isValid()) {
        for (auto& ns : flc->project->registry().allNamespaces()) {
            auto* sym = flc->project->registry().findSymbol(ns, funcName);
            if (!sym) continue;
            if (sym->kind == ExportedSymbol::Function) {
                auto* fd = dynamic_cast<LuxParser::FunctionDeclContext*>(sym->decl);
                if (fd) return safeText(fd->typeSpec());
            }
        }
    }

    // 4) Builtins
    if (flc->builtinReg) {
        auto* builtin = flc->builtinReg->lookup(funcName);
        if (builtin) return builtin->returnType;
    }

    return "";
}

static LuxParser::FunctionDeclContext* findFunctionDeclForInference(
    const std::string& name,
    const FuncLookupCtx* flc) {
    if (!flc) return nullptr;
    if (flc->tree) {
        for (auto* tld : flc->tree->topLevelDecl()) {
            auto* fd = tld->functionDecl();
            if (fd && !fd->IDENTIFIER().empty() && safeIdAt(fd, 0) == name)
                return fd;
        }
    }
    if (flc->project && flc->project->isValid()) {
        for (auto& ns : flc->project->registry().allNamespaces()) {
            auto* sym = flc->project->registry().findSymbol(ns, name);
            if (!sym || sym->kind != ExportedSymbol::Function) continue;
            if (auto* fd = dynamic_cast<LuxParser::FunctionDeclContext*>(sym->decl))
                return fd;
        }
    }
    return nullptr;
}

static LuxParser::EnumDeclContext* findEnumDeclForInference(
    const std::string& name,
    const FuncLookupCtx* flc) {
    if (!flc) return nullptr;
    if (flc->tree) {
        for (auto* tld : flc->tree->topLevelDecl()) {
            auto* ed = tld->enumDecl();
            if (ed && ed->IDENTIFIER() && safeText(ed->IDENTIFIER()) == name)
                return ed;
        }
    }
    if (flc->project && flc->project->isValid()) {
        for (auto& ns : flc->project->registry().allNamespaces()) {
            auto* sym = flc->project->registry().findSymbol(ns, name);
            if (!sym || sym->kind != ExportedSymbol::Enum) continue;
            if (auto* ed = dynamic_cast<LuxParser::EnumDeclContext*>(sym->decl))
                return ed;
        }
    }
    return nullptr;
}

static LuxParser::StructDeclContext* findStructDeclForInference(
    const std::string& name,
    const FuncLookupCtx* flc) {
    if (!flc) return nullptr;
    if (flc->tree) {
        for (auto* tld : flc->tree->topLevelDecl()) {
            auto* sd = tld->structDecl();
            if (sd && sd->IDENTIFIER() && safeText(sd->IDENTIFIER()) == name)
                return sd;
        }
    }
    if (flc->project && flc->project->isValid()) {
        for (auto& ns : flc->project->registry().allNamespaces()) {
            auto* sym = flc->project->registry().findSymbol(ns, name);
            if (!sym || sym->kind != ExportedSymbol::Struct) continue;
            if (auto* sd = dynamic_cast<LuxParser::StructDeclContext*>(sym->decl))
                return sd;
        }
    }
    return nullptr;
}

static LuxParser::UnionDeclContext* findUnionDeclForInference(
    const std::string& name,
    const FuncLookupCtx* flc) {
    if (!flc) return nullptr;
    if (flc->tree) {
        for (auto* tld : flc->tree->topLevelDecl()) {
            auto* ud = tld->unionDecl();
            if (ud && ud->IDENTIFIER() && safeText(ud->IDENTIFIER()) == name)
                return ud;
        }
    }
    if (flc->project && flc->project->isValid()) {
        for (auto& ns : flc->project->registry().allNamespaces()) {
            auto* sym = flc->project->registry().findSymbol(ns, name);
            if (!sym || sym->kind != ExportedSymbol::Union) continue;
            if (auto* ud = dynamic_cast<LuxParser::UnionDeclContext*>(sym->decl))
                return ud;
        }
    }
    return nullptr;
}

// Infer the type name of an expression for auto variable resolution.
// This is a lightweight heuristic — no full type-checking.
static std::string inferExprTypeName(
        LuxParser::ExpressionContext* expr,
        const std::unordered_map<std::string, HoverProvider::LocalVar>& locals,
        const FuncLookupCtx* flc = nullptr) {
    if (!expr) return "";

    // Literals
    if (dynamic_cast<LuxParser::IntLitExprContext*>(expr) ||
        dynamic_cast<LuxParser::HexLitExprContext*>(expr) ||
        dynamic_cast<LuxParser::OctLitExprContext*>(expr) ||
        dynamic_cast<LuxParser::BinLitExprContext*>(expr))   return "int32";
    if (dynamic_cast<LuxParser::FloatLitExprContext*>(expr)) return "float64";
    if (dynamic_cast<LuxParser::BoolLitExprContext*>(expr))  return "bool";
    if (dynamic_cast<LuxParser::CharLitExprContext*>(expr))  return "char";
    if (dynamic_cast<LuxParser::StrLitExprContext*>(expr))   return "string";
    if (dynamic_cast<LuxParser::CStrLitExprContext*>(expr))  return "*char";

    // Identifier: look up in collected locals
    if (auto* id = dynamic_cast<LuxParser::IdentExprContext*>(expr)) {
        auto it = locals.find(safeText(id->IDENTIFIER()));
        if (it != locals.end()) return it->second.typeName;
        return "";
    }

    auto resolveFieldType = [&](const std::string& ownerType,
                                const std::string& fieldName) -> std::string {
        if (ownerType.empty()) return "";

        std::string lookupType = ownerType;
        std::unordered_map<std::string, std::string> subst;
        std::string base;
        std::vector<std::string> args;
        if (parseGenericInstance(ownerType, base, args)) {
            lookupType = base;
        }

        if (auto* sd = findStructDeclForInference(lookupType, flc)) {
            if (!args.empty() && sd->typeParamList()) {
                auto tps = sd->typeParamList()->typeParam();
                for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
                    auto ids = tps[i]->IDENTIFIER();
                    if (!ids.empty()) subst[ids[0]->getText()] = args[i];
                }
            }
            for (auto* f : sd->structField()) {
                if (safeText(f->IDENTIFIER()) == fieldName)
                    return substituteTypeParams(safeText(f->typeSpec()), subst);
            }
        }

        if (auto* ud = findUnionDeclForInference(lookupType, flc)) {
            if (!args.empty() && ud->typeParamList()) {
                auto tps = ud->typeParamList()->typeParam();
                for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
                    auto ids = tps[i]->IDENTIFIER();
                    if (!ids.empty()) subst[ids[0]->getText()] = args[i];
                }
            }
            for (auto* f : ud->unionField()) {
                if (safeText(f->IDENTIFIER()) == fieldName)
                    return substituteTypeParams(safeText(f->typeSpec()), subst);
            }
        }

        if (flc && flc->bindings) {
            if (auto* cs = flc->bindings->findStruct(lookupType)) {
                for (auto& f : cs->fields) {
                    if (f.name == fieldName && f.typeInfo)
                        return f.typeInfo->name;
                }
            }
        }

        return "";
    };

    if (auto* fa = dynamic_cast<LuxParser::FieldAccessExprContext*>(expr)) {
        auto baseType = inferExprTypeName(fa->expression(), locals, flc);
        if (baseType.empty()) return "";
        // Auto-dereference pointer for field access (e.g. n2.next.value)
        while (!baseType.empty() && baseType[0] == '*')
            baseType = baseType.substr(1);
        return resolveFieldType(baseType, safeText(fa->IDENTIFIER()));
    }

    if (auto* aa = dynamic_cast<LuxParser::ArrowAccessExprContext*>(expr)) {
        auto baseType = inferExprTypeName(aa->expression(), locals, flc);
        if (baseType.empty() || baseType[0] != '*') return "";
        return resolveFieldType(baseType.substr(1), safeText(aa->IDENTIFIER()));
    }

    // Address-of: &expr → *type
    if (auto* addr = dynamic_cast<LuxParser::AddrOfExprContext*>(expr)) {
        if (auto* ident = dynamic_cast<LuxParser::IdentExprContext*>(addr->expression())) {
            auto it = locals.find(safeText(ident->IDENTIFIER()));
            if (it != locals.end()) return "*" + it->second.typeName;
        }
        return "";
    }

    // Function call: resolve return type
    if (auto* fn = dynamic_cast<LuxParser::FnCallExprContext*>(expr)) {
        if (auto* ident = dynamic_cast<LuxParser::IdentExprContext*>(fn->expression())) {
            auto funcName = safeText(ident->IDENTIFIER());

            if (auto* fd = findFunctionDeclForInference(funcName, flc)) {
                if (!fd->typeParamList()) {
                    auto ret = safeText(fd->typeSpec());
                    if (!ret.empty()) return ret;
                } else {
                    std::unordered_set<std::string> tps;
                    for (auto* tp : fd->typeParamList()->typeParam()) {
                        auto ids = tp->IDENTIFIER();
                        if (!ids.empty()) tps.insert(ids[0]->getText());
                    }

                    std::vector<std::string> actualArgTypes;
                    if (auto* al = fn->argList()) {
                        for (auto* a : al->expression())
                            actualArgTypes.push_back(inferExprTypeName(a, locals, flc));
                    }

                    std::vector<LuxParser::ParamContext*> formalParams;
                    if (auto* pl = fd->paramList()) formalParams = pl->param();
                    if (actualArgTypes.size() == formalParams.size()) {
                        std::unordered_map<std::string, std::string> inferred;
                        bool ok = true;
                        for (size_t i = 0; i < formalParams.size(); i++) {
                            auto formal = safeText(formalParams[i]->typeSpec());
                            auto actual = actualArgTypes[i];
                            if (actual.empty()) { ok = false; break; }
                            if (!unifyGenericType(formal, actual, tps, inferred)) {
                                ok = false;
                                break;
                            }
                        }
                        for (auto& tp : tps) {
                            if (!inferred.count(tp)) { ok = false; break; }
                        }
                        if (ok) {
                            auto ret = substituteTypeParams(safeText(fd->typeSpec()), inferred);
                            if (!ret.empty()) return ret;
                        }
                    }
                }
            }

            auto ret = lookupFuncReturnType(funcName, flc);
            if (!ret.empty()) return ret;
        }
        return "";
    }

    // Generic function call: d<string>(...) → substitute explicit type args
    if (auto* gfc = dynamic_cast<LuxParser::GenericFnCallExprContext*>(expr)) {
        auto* fnId = gfc->IDENTIFIER();
        if (!fnId) return "";
        std::string funcName = fnId->getText();

        if (auto* fd = findFunctionDeclForInference(funcName, flc)) {
            if (!fd->typeParamList()) {
                auto ret = safeText(fd->typeSpec());
                if (!ret.empty()) return ret;
            } else {
                std::unordered_map<std::string, std::string> subst;
                auto tps = fd->typeParamList()->typeParam();
                auto args = gfc->typeSpec();
                for (size_t i = 0; i < std::min(tps.size(), args.size()); i++) {
                    auto ids = tps[i]->IDENTIFIER();
                    if (!ids.empty()) subst[ids[0]->getText()] = args[i]->getText();
                }

                auto ret = substituteTypeParams(safeText(fd->typeSpec()), subst);
                if (!ret.empty()) return ret;
            }
        }

        auto ret = lookupFuncReturnType(funcName, flc);
        if (!ret.empty()) return ret;
        return "";
    }

    // Enum access: Direction::North → "Direction"
    if (auto* ea = dynamic_cast<LuxParser::EnumAccessExprContext*>(expr)) {
        auto ids = ea->IDENTIFIER();
        if (!ids.empty()) return ids[0]->getText();
        return "";
    }

    // Generic enum access: Result<int32, string>::Unit → "Result<int32,string>"
    if (auto* gea = dynamic_cast<LuxParser::GenericEnumAccessExprContext*>(expr)) {
        auto ids = gea->IDENTIFIER();
        if (ids.empty()) return "";

        std::string outType = ids[0]->getText() + "<";
        auto args = gea->typeSpec();
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) outType += ",";
            outType += args[i]->getText();
        }
        outType += ">";
        return outType;
    }

    // Static method call: Type::method(...) → look up return type from extend blocks
    if (auto* smc = dynamic_cast<LuxParser::StaticMethodCallExprContext*>(expr)) {
        auto ids = smc->IDENTIFIER();
        if (ids.size() >= 2) {
            std::string ownerPath;
            for (size_t i = 0; i + 1 < ids.size(); ++i) {
                if (!ownerPath.empty()) ownerPath += "::";
                ownerPath += ids[i]->getText();
            }

            std::string typeName = ownerPath;
            auto lastScope = typeName.rfind("::");
            if (lastScope != std::string::npos)
                typeName = typeName.substr(lastScope + 2);

            std::string methodName = ids.back()->getText();

            // Intrinsic calls: lux::core::trap()
            if (flc && flc->intrinsicReg &&
                IntrinsicRegistry::isIntrinsicPrefix(ids.front()->getText())) {
                std::vector<std::string> idTexts;
                for (auto* id : ids) idTexts.push_back(id->getText());
                std::string ns, funcName;
                if (IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName)) {
                    if (auto* intrinsic = flc->intrinsicReg->lookup(ns, funcName))
                        return intrinsic->returnType;
                }
            }

            if (NamespaceRegistry::isStdModule(ownerPath) && flc && flc->builtinReg) {
                if (auto* sig = flc->builtinReg->lookup(methodName))
                    return sig->returnType;
            }

            // Check user-defined extend blocks in current file
            if (flc && flc->tree) {
                for (auto* tld : flc->tree->topLevelDecl()) {
                    auto* ext = tld->extendDecl();
                    if (!ext) continue;
                    if (safeText(ext->IDENTIFIER()) != typeName) continue;
                    for (auto* m : ext->extendMethod()) {
                        if (safeIdAt(m, 0) != methodName) continue;

                        if (!ext->typeParamList())
                            return safeText(m->typeSpec());

                        std::unordered_set<std::string> tps;
                        for (auto* tp : ext->typeParamList()->typeParam()) {
                            auto tpIds = tp->IDENTIFIER();
                            if (!tpIds.empty()) tps.insert(tpIds[0]->getText());
                        }

                        std::vector<std::string> actualArgTypes;
                        if (auto* al = smc->argList()) {
                            for (auto* a : al->expression())
                                actualArgTypes.push_back(inferExprTypeName(a, locals, flc));
                        }

                        std::vector<LuxParser::ParamContext*> formalParams;
                        if (auto* pl = m->paramList()) formalParams = pl->param();
                        if (actualArgTypes.size() != formalParams.size())
                            return safeText(m->typeSpec());

                        std::unordered_map<std::string, std::string> inferred;
                        bool ok = true;
                        for (size_t i = 0; i < formalParams.size(); i++) {
                            auto formal = safeText(formalParams[i]->typeSpec());
                            auto actual = actualArgTypes[i];
                            if (actual.empty()) { ok = false; break; }
                            if (!unifyGenericType(formal, actual, tps, inferred)) {
                                ok = false;
                                break;
                            }
                        }
                        for (auto& tp : tps) {
                            if (!inferred.count(tp)) { ok = false; break; }
                        }

                        if (ok)
                            return substituteTypeParams(safeText(m->typeSpec()), inferred);
                        return safeText(m->typeSpec());
                    }
                }
            }

            // Check cross-file extend blocks
            if (flc && flc->project && flc->project->isValid()) {
                for (auto& ns : flc->project->registry().allNamespaces()) {
                    auto syms = flc->project->registry().getNamespaceSymbols(ns);
                    for (auto* sym : syms) {
                        if (sym->kind != ExportedSymbol::ExtendBlock) continue;
                        auto* ext = dynamic_cast<LuxParser::ExtendDeclContext*>(sym->decl);
                        if (!ext || safeText(ext->IDENTIFIER()) != typeName) continue;
                        for (auto* m : ext->extendMethod()) {
                            if (safeIdAt(m, 0) == methodName)
                                return safeText(m->typeSpec());
                        }
                    }
                }
            }

            // Check extended type registry (Vec, Map, Set)
            if (flc && flc->extTypeReg) {
                auto* desc = flc->extTypeReg->lookup(typeName);
                if (desc) {
                    for (auto& md : desc->methods) {
                        if (md.name == methodName) return md.returnType;
                    }
                }
            }
        }
        return "";
    }

    // Method call: resolve receiver type, then look up method return type
    if (auto* mc = dynamic_cast<LuxParser::MethodCallExprContext*>(expr)) {
        auto receiverType = inferExprTypeName(mc->expression(), locals, flc);
        if (!receiverType.empty()) {
            std::string methodName = safeText(mc->IDENTIFIER());

            if (!receiverType.empty() && receiverType[0] == '[' && flc && flc->methodReg) {
                auto* md = flc->methodReg->lookupArrayMethod(methodName);
                if (md) {
                    auto arrayElemType = [](const std::string& t) -> std::string {
                        if (t.rfind("[]", 0) == 0) return t.substr(2);
                        if (!t.empty() && t[0] == '[') {
                            auto close = t.find(']');
                            if (close != std::string::npos && close + 1 < t.size())
                                return t.substr(close + 1);
                        }
                        return "";
                    };

                    if (md->returnType == "_self") return receiverType;
                    if (md->returnType == "_elem") return arrayElemType(receiverType);
                    return md->returnType;
                }
            }

            // 1) Check user-defined extend methods
            if (flc && flc->tree) {
                for (auto* tld : flc->tree->topLevelDecl()) {
                    auto* ext = tld->extendDecl();
                    if (!ext) continue;
                    if (safeText(ext->IDENTIFIER()) != receiverType) continue;
                    for (auto* m : ext->extendMethod()) {
                        if (safeIdAt(m, 0) == methodName)
                            return safeText(m->typeSpec());
                    }
                }
            }

            // 1b) Check cross-file extend methods
            if (flc && flc->project && flc->project->isValid()) {
                for (auto& ns : flc->project->registry().allNamespaces()) {
                    auto syms = flc->project->registry().getNamespaceSymbols(ns);
                    for (auto* sym : syms) {
                        if (sym->kind != ExportedSymbol::ExtendBlock) continue;
                        auto* ext = dynamic_cast<LuxParser::ExtendDeclContext*>(sym->decl);
                        if (!ext || safeText(ext->IDENTIFIER()) != receiverType) continue;
                        for (auto* m : ext->extendMethod()) {
                            if (safeIdAt(m, 0) == methodName)
                                return safeText(m->typeSpec());
                        }
                    }
                }
            }

            // 2) Check builtin extended type methods (Vec, Map, Set, etc.)
            if (flc && flc->extTypeReg) {
                // Extract base name and generic args: "map<string, int32>" → "map", ["string", "int32"]
                std::string baseName = receiverType;
                std::vector<std::string> typeArgs;
                auto ltPos = receiverType.find('<');
                if (ltPos != std::string::npos) {
                    baseName = receiverType.substr(0, ltPos);
                    auto gtPos = receiverType.rfind('>');
                    if (gtPos != std::string::npos) {
                        std::string inner = receiverType.substr(ltPos + 1, gtPos - ltPos - 1);
                        int depth = 0; size_t start = 0;
                        for (size_t i = 0; i <= inner.size(); ++i) {
                            if (i == inner.size() || (inner[i] == ',' && depth == 0)) {
                                auto arg = inner.substr(start, i - start);
                                size_t b = arg.find_first_not_of(' ');
                                size_t e = arg.find_last_not_of(' ');
                                if (b != std::string::npos)
                                    typeArgs.push_back(arg.substr(b, e - b + 1));
                                start = i + 1;
                            } else if (inner[i] == '<') ++depth;
                            else if (inner[i] == '>') --depth;
                        }
                    }
                }

                auto* desc = flc->extTypeReg->lookup(normalizeExtBaseName(baseName));
                if (desc) {
                    for (auto& md : desc->methods) {
                        if (md.name != methodName) continue;
                        // Resolve placeholder return type
                        auto& rt = md.returnType;
                        if (rt == "_self")  return receiverType;
                        if (rt == "_elem" && !typeArgs.empty()) return typeArgs[0];
                        if (rt == "_key"  && !typeArgs.empty()) return typeArgs[0];
                        if (rt == "_val"  && typeArgs.size() >= 2) return typeArgs[1];
                        if (rt == "_val"  && typeArgs.size() == 1) return typeArgs[0];
                        if (rt == "_vec_key" && !typeArgs.empty()) return "vec<" + typeArgs[0] + ">";
                        if (rt == "_vec_val" && typeArgs.size() >= 2) return "vec<" + typeArgs[1] + ">";
                        if (rt == "_vec_val" && typeArgs.size() == 1) return "vec<" + typeArgs[0] + ">";
                        return rt; // "void", "bool", "usize", etc.
                    }
                }
            }
        }
        return "";
    }

    // Dereference: *expr → remove one *
    if (auto* deref = dynamic_cast<LuxParser::DerefExprContext*>(expr)) {
        auto inner = inferExprTypeName(deref->expression(), locals, flc);
        if (inner.size() > 1 && inner[0] == '*') return inner.substr(1);
        return "";
    }

    // Negation: -expr → same type
    if (auto* neg = dynamic_cast<LuxParser::NegExprContext*>(expr))
        return inferExprTypeName(neg->expression(), locals, flc);

    // Logical NOT → bool
    if (dynamic_cast<LuxParser::LogicalNotExprContext*>(expr)) return "bool";

    // Comparisons → bool
    if (dynamic_cast<LuxParser::RelExprContext*>(expr))        return "bool";
    if (dynamic_cast<LuxParser::EqExprContext*>(expr))         return "bool";
    if (dynamic_cast<LuxParser::LogicalAndExprContext*>(expr)) return "bool";
    if (dynamic_cast<LuxParser::LogicalOrExprContext*>(expr))  return "bool";

    // Binary arithmetic: use left operand type
    if (auto* mul = dynamic_cast<LuxParser::MulExprContext*>(expr))
        return inferExprTypeName(mul->expression(0), locals, flc);
    if (auto* add = dynamic_cast<LuxParser::AddSubExprContext*>(expr))
        return inferExprTypeName(add->expression(0), locals, flc);

    // Paren: unwrap
    if (auto* paren = dynamic_cast<LuxParser::ParenExprContext*>(expr))
        return inferExprTypeName(paren->expression(), locals, flc);

    // Cast: target type
    if (auto* cast = dynamic_cast<LuxParser::CastExprContext*>(expr))
        return safeText(cast->typeSpec());

    // Is expression always returns bool
    if (dynamic_cast<LuxParser::IsExprContext*>(expr))
        return "bool";

    // Ternary: type of true branch
    if (auto* tern = dynamic_cast<LuxParser::TernaryExprContext*>(expr))
        return inferExprTypeName(tern->expression(1), locals, flc);

    if (auto* cu = dynamic_cast<LuxParser::CatchUnwrapExprContext*>(expr)) {
        auto sourceType = inferExprTypeName(cu->expression(), locals, flc);
        if (sourceType.empty() || !flc) return "";

        std::string baseName = sourceType;
        std::vector<std::string> sourceArgs;
        parseGenericInstance(sourceType, baseName, sourceArgs);

        auto* enumDecl = findEnumDeclForInference(baseName, flc);
        if (!enumDecl) return "";

        std::unordered_map<std::string, std::string> subst;
        if (enumDecl->typeParamList() && !sourceArgs.empty()) {
            auto tps = enumDecl->typeParamList()->typeParam();
            for (size_t i = 0; i < std::min(tps.size(), sourceArgs.size()); i++) {
                auto ids = tps[i]->IDENTIFIER();
                if (!ids.empty()) subst[ids[0]->getText()] = sourceArgs[i];
            }
        }

        std::string successType;
        bool seenError = false;
        for (auto* variant : enumDecl->enumVariant()) {
            if (!variant || variant->typeSpec().size() != 1) return "";
            auto payloadType = variant->typeSpec(0)->getText();
            payloadType = substituteTypeParams(payloadType, subst);
            if (payloadType == "Error") {
                if (seenError) return "";
                seenError = true;
            } else {
                if (!successType.empty()) return "";
                successType = payloadType;
            }
        }

        if (!seenError || successType.empty()) return "";
        return successType;
    }

    // Array literal: [expr, ...] → infer element type, prepend []
    if (auto* arr = dynamic_cast<LuxParser::ArrayLitExprContext*>(expr)) {
        auto elems = arr->expression();
        if (!elems.empty()) {
            auto elemType = inferExprTypeName(elems[0], locals, flc);
            if (!elemType.empty()) return "[]" + elemType;
        }
        return "";
    }

    // Struct literal: Point { x: 10, y: 20 } → "Point"
    if (auto* sl = dynamic_cast<LuxParser::StructLitExprContext*>(expr)) {
        auto ids = sl->IDENTIFIER();
        if (!ids.empty()) return ids[0]->getText();
        return "";
    }

    // Generic struct/union literal: Result<int32, string> { ... } → "Result<int32,string>"
    if (auto* gsl = dynamic_cast<LuxParser::GenericStructLitExprContext*>(expr)) {
        if (!gsl->IDENTIFIER().empty()) {
            std::string base = safeIdAt(gsl, 0);
            std::string outType = base + "<";
            auto args = gsl->typeSpec();
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) outType += ",";
                outType += args[i]->getText();
            }
            outType += ">";
            return outType;
        }
        return "";
    }

    // Sizeof → int64
    if (dynamic_cast<LuxParser::SizeofExprContext*>(expr)) return "int64";

    // Typeof → string
    if (dynamic_cast<LuxParser::TypeofExprContext*>(expr)) return "string";

    // Tuple literal: (10, 20) → "tuple<int32, int32>"
    if (auto* tup = dynamic_cast<LuxParser::TupleLitExprContext*>(expr)) {
        auto exprs = tup->expression();
        std::string result = "tuple<";
        for (size_t i = 0; i < exprs.size(); i++) {
            if (i > 0) result += ", ";
            auto elemType = inferExprTypeName(exprs[i], locals, flc);
            result += elemType.empty() ? "auto" : elemType;
        }
        result += ">";
        return result;
    }

    // Index: expr[i] → element type
    if (auto* idx = dynamic_cast<LuxParser::IndexExprContext*>(expr)) {
        auto exprs = idx->expression();
        if (!exprs.empty()) {
            auto baseType = inferExprTypeName(exprs[0], locals, flc);
            if (baseType == "string") return "char";
            // Array type [N]T or []T: strip leading [...]
            if (!baseType.empty() && baseType[0] == '[') {
                auto close = baseType.find(']');
                if (close != std::string::npos && close + 1 < baseType.size())
                    return baseType.substr(close + 1);
            }
        }
        return "";
    }

    // Tuple index: expr.N → element type
    if (auto* ti = dynamic_cast<LuxParser::TupleIndexExprContext*>(expr)) {
        auto baseType = inferExprTypeName(ti->expression(), locals, flc);
        auto resolved = resolveTupleTypeName(baseType, flc);
        if (!resolved.empty()) {
            auto elems = parseTupleElements(resolved);
            unsigned idx = std::stoul(ti->INT_LIT()->getText());
            if (idx < elems.size()) return elems[idx];
        }
        return "";
    }

    // Chained tuple index: expr.N.M (FLOAT_LIT) → inner element type
    if (auto* cti = dynamic_cast<LuxParser::ChainedTupleIndexExprContext*>(expr)) {
        auto baseType = inferExprTypeName(cti->expression(), locals, flc);
        auto resolved = resolveTupleTypeName(baseType, flc);
        if (!resolved.empty()) {
            auto elems = parseTupleElements(resolved);
            auto text = cti->FLOAT_LIT()->getText();
            auto dotPos = text.find('.');
            unsigned idx1 = std::stoul(text.substr(0, dotPos));
            if (idx1 < elems.size()) {
                auto innerResolved = resolveTupleTypeName(elems[idx1], flc);
                if (!innerResolved.empty()) {
                    auto innerElems = parseTupleElements(innerResolved);
                    unsigned idx2 = std::stoul(text.substr(dotPos + 1));
                    if (idx2 < innerElems.size()) return innerElems[idx2];
                }
            }
        }
        return "";
    }

    // Tuple arrow index: expr->N → element type
    if (auto* tai = dynamic_cast<LuxParser::TupleArrowIndexExprContext*>(expr)) {
        auto baseType = inferExprTypeName(tai->expression(), locals, flc);
        // Strip leading * from pointer type
        std::string pointeeType = baseType;
        if (!pointeeType.empty() && pointeeType[0] == '*')
            pointeeType = pointeeType.substr(1);
        auto resolved = resolveTupleTypeName(pointeeType, flc);
        if (!resolved.empty()) {
            auto elems = parseTupleElements(resolved);
            unsigned idx = std::stoul(tai->INT_LIT()->getText());
            if (idx < elems.size()) return elems[idx];
        }
        return "";
    }

    // Chained tuple arrow index: expr->N.M (FLOAT_LIT) → inner element type
    if (auto* ctai = dynamic_cast<LuxParser::ChainedTupleArrowIndexExprContext*>(expr)) {
        auto baseType = inferExprTypeName(ctai->expression(), locals, flc);
        std::string pointeeType = baseType;
        if (!pointeeType.empty() && pointeeType[0] == '*')
            pointeeType = pointeeType.substr(1);
        auto resolved = resolveTupleTypeName(pointeeType, flc);
        if (!resolved.empty()) {
            auto elems = parseTupleElements(resolved);
            auto text = ctai->FLOAT_LIT()->getText();
            auto dotPos = text.find('.');
            unsigned idx1 = std::stoul(text.substr(0, dotPos));
            if (idx1 < elems.size()) {
                auto innerResolved = resolveTupleTypeName(elems[idx1], flc);
                if (!innerResolved.empty()) {
                    auto innerElems = parseTupleElements(innerResolved);
                    unsigned idx2 = std::stoul(text.substr(dotPos + 1));
                    if (idx2 < innerElems.size()) return innerElems[idx2];
                }
            }
        }
        return "";
    }

    return "";
}

static std::string inferCatchUnwrapItType(
        LuxParser::ExpressionContext* sourceExpr,
        LuxParser::ProgramContext* tree,
        const std::unordered_map<std::string, HoverProvider::LocalVar>& locals,
        const FuncLookupCtx* flc) {
    if (!sourceExpr) return "";

    auto enumTypeName = inferExprTypeName(sourceExpr, locals, flc);
    if (enumTypeName.empty()) return "";

    auto baseName = enumTypeName;
    auto lt = baseName.find('<');
    if (lt != std::string::npos)
        baseName = baseName.substr(0, lt);

    auto* enumDecl = findEnumDeclForInference(baseName, flc);
    if (!enumDecl && tree) {
        for (auto* tld : tree->topLevelDecl()) {
            if (auto* ed = tld->enumDecl();
                ed && ed->IDENTIFIER() && safeText(ed->IDENTIFIER()) == baseName) {
                enumDecl = ed;
                break;
            }
        }
    }
    if (!enumDecl) return "";

    std::unordered_map<std::string, std::string> subst;
    if (enumDecl->typeParamList()) {
        std::vector<std::string> sourceArgs;
        std::string parsedBase = enumTypeName;
        parseGenericInstance(enumTypeName, parsedBase, sourceArgs);
        auto tps = enumDecl->typeParamList()->typeParam();
        for (size_t i = 0; i < std::min(tps.size(), sourceArgs.size()); i++) {
            auto ids = tps[i]->IDENTIFIER();
            if (!ids.empty())
                subst[ids[0]->getText()] = sourceArgs[i];
        }
    }

    std::string errType;
    for (auto* variant : enumDecl->enumVariant()) {
        if (!variant || variant->typeSpec().size() != 1) continue;
        auto payloadName = variant->typeSpec(0)->getText();
        auto variantName = variant->IDENTIFIER() ? safeText(variant->IDENTIFIER()) : "";
        bool isErrName = variantName == "Err" || variantName == "Error" ||
                         variantName == "Failure" || variantName == "Fail" ||
                         variantName == "None";
        if (isErrName || payloadName == "Error") {
            errType = substituteTypeParams(payloadName, subst);
        }
    }

    return errType.empty() ? "Error" : errType;
}

static void collectLocalsFromStmts(
        const std::vector<LuxParser::StatementContext*>& stmts,
        size_t beforeLine,
        std::unordered_map<std::string, HoverProvider::LocalVar>& out,
        const FuncLookupCtx* flc = nullptr) {

    auto cursorInsideNode = [](antlr4::ParserRuleContext* node, size_t cursorLine0) {
        if (!node) return false;
        auto* start = node->getStart();
        auto* stop  = node->getStop();
        if (!start || !stop) return false;
        size_t startLine = start->getLine() - 1;
        size_t stopLine  = stop->getLine() - 1;
        return cursorLine0 >= startLine && cursorLine0 <= stopLine;
    };

    auto inferIsBindingPayloadType = [&](LuxParser::ExpressionContext* cond) -> std::string {
        auto* isE = dynamic_cast<LuxParser::IsExprContext*>(cond);
        if (!isE || !isE->SCOPE() || !isE->LPAREN() || !isE->IDENTIFIER(1)) return "";

        auto* rhsType = isE->typeSpec();
        if (!rhsType || !rhsType->IDENTIFIER() || !flc || !flc->tree) return "";

        auto enumName = safeText(rhsType->IDENTIFIER());
        LuxParser::EnumDeclContext* enumDecl = nullptr;
        for (auto* tld : flc->tree->topLevelDecl()) {
            if (auto* ed = tld->enumDecl(); ed && ed->IDENTIFIER() &&
                safeText(ed->IDENTIFIER()) == enumName) {
                enumDecl = ed;
                break;
            }
        }
        if (!enumDecl || !isE->IDENTIFIER(0)) return "";

        auto variantName = safeIdAt(isE, 0);
        for (auto* variant : enumDecl->enumVariant()) {
            auto* vId = variant->IDENTIFIER();
            if (!vId || vId->getText() != variantName) continue;
            if (!variant->LPAREN() || variant->typeSpec().size() != 1) return "";

            auto payloadType = variant->typeSpec(0)->getText();
            if (rhsType->LT() && enumDecl->typeParamList()) {
                std::unordered_map<std::string, std::string> subst;
                auto tps = enumDecl->typeParamList()->typeParam();
                auto args = rhsType->typeSpec();
                for (size_t i = 0; i < tps.size() && i < args.size(); i++) {
                    auto ids = tps[i]->IDENTIFIER();
                    if (!ids.empty()) subst[ids[0]->getText()] = args[i]->getText();
                }
                payloadType = substituteTypeParams(payloadType, subst);
            }
            return payloadType;
        }

        return "";
    };

    for (auto* stmt : stmts) {
        // Stop collecting if statement starts after the cursor line
        auto* start = stmt->getStart();
        if (start && start->getLine() > beforeLine + 1) break;

        if (auto* vd = stmt->varDeclStmt()) {
            // ── Tuple destructuring: auto (x, y) = expr; ────────────
            if (vd->LPAREN()) {
                auto ids = vd->IDENTIFIER();
                std::string initType;
                if (vd->expression())
                    initType = inferExprTypeName(vd->expression(), out, flc);
                auto resolved = resolveTupleTypeName(initType, flc);
                auto elems = parseTupleElements(resolved);
                for (size_t i = 0; i < ids.size(); i++) {
                    std::string elemType = (i < elems.size()) ? elems[i] : "auto";
                    out[ids[i]->getText()] = {elemType, 0};
                }
            } else {
                // ── Normal variable declaration ──────────────────────
                std::string typeName;
                if (vd->typeSpec())
                    typeName = safeText(vd->typeSpec());
                // Resolve 'auto' to the inferred type from the initializer
                if (typeName == "auto" && vd->expression()) {
                    auto inferred = inferExprTypeName(vd->expression(), out, flc);
                    if (!inferred.empty()) typeName = inferred;
                }
                std::string varName = !vd->IDENTIFIER().empty() ? safeIdAt(vd, 0) : "";
                if (!varName.empty())
                    out[varName] = {typeName, 0};
            }

            if (auto* cu = dynamic_cast<LuxParser::CatchUnwrapExprContext*>(vd->expression())) {
                if (cursorInsideNode(cu->block(), beforeLine)) {
                    auto itType = inferCatchUnwrapItType(cu->expression(), flc ? flc->tree : nullptr, out, flc);
                    out["it"] = {itType, 0};
                    collectLocalsFromBlock(cu->block(), beforeLine, out, flc);
                }
            }
        }

        // Recurse into nested blocks
        if (auto* ifS = stmt->ifStmt()) {
            if (auto* body = ifS->ifBody()) {
                if (auto* b = body->block()) {
                    if (cursorInsideNode(b, beforeLine)) {
                        if (auto* isE = dynamic_cast<LuxParser::IsExprContext*>(ifS->expression());
                            isE && isE->IDENTIFIER(1)) {
                            auto bindType = inferIsBindingPayloadType(ifS->expression());
                            out[safeIdAt(isE, 1)] = {bindType.empty() ? "auto" : bindType, 0};
                        }
                        collectLocalsFromBlock(b, beforeLine, out, flc);
                    }
                } else if (auto* s = body->statement()) {
                    if (cursorInsideNode(s, beforeLine)) {
                        if (auto* isE = dynamic_cast<LuxParser::IsExprContext*>(ifS->expression());
                            isE && isE->IDENTIFIER(1)) {
                            auto bindType = inferIsBindingPayloadType(ifS->expression());
                            out[safeIdAt(isE, 1)] = {bindType.empty() ? "auto" : bindType, 0};
                        }
                        collectLocalsFromStmts({s}, beforeLine, out, flc);
                    }
                }
            }
            for (auto* elif : ifS->elseIfClause()) {
                if (auto* body = elif->ifBody()) {
                    if (auto* b = body->block()) {
                        if (cursorInsideNode(b, beforeLine)) {
                            if (auto* isE = dynamic_cast<LuxParser::IsExprContext*>(elif->expression());
                                isE && isE->IDENTIFIER(1)) {
                                auto bindType = inferIsBindingPayloadType(elif->expression());
                                out[safeIdAt(isE, 1)] = {bindType.empty() ? "auto" : bindType, 0};
                            }
                            collectLocalsFromBlock(b, beforeLine, out, flc);
                        }
                    } else if (auto* s = body->statement()) {
                        if (cursorInsideNode(s, beforeLine)) {
                            if (auto* isE = dynamic_cast<LuxParser::IsExprContext*>(elif->expression());
                                isE && isE->IDENTIFIER(1)) {
                                auto bindType = inferIsBindingPayloadType(elif->expression());
                                out[safeIdAt(isE, 1)] = {bindType.empty() ? "auto" : bindType, 0};
                            }
                            collectLocalsFromStmts({s}, beforeLine, out, flc);
                        }
                    }
                }
            }
            if (ifS->elseClause()) {
                if (auto* body = ifS->elseClause()->ifBody()) {
                    if (auto* b = body->block()) {
                        if (cursorInsideNode(b, beforeLine))
                            collectLocalsFromBlock(b, beforeLine, out, flc);
                    } else if (auto* s = body->statement()) {
                        if (cursorInsideNode(s, beforeLine))
                            collectLocalsFromStmts({s}, beforeLine, out, flc);
                    }
                }
            }
        }
        if (auto* forIn = stmt->forStmt()) {
            if (auto* fin = dynamic_cast<LuxParser::ForInStmtContext*>(forIn)) {
                if (fin->typeSpec() && fin->IDENTIFIER()) {
                    std::string tname = safeText(fin->typeSpec());
                    out[safeText(fin->IDENTIFIER())] = {tname, 0};
                }
                collectLocalsFromBlock(fin->block(), beforeLine, out, flc);
            }
            if (auto* fc = dynamic_cast<LuxParser::ForClassicStmtContext*>(forIn)) {
                if (fc->typeSpec() && fc->IDENTIFIER()) {
                    std::string tname = safeText(fc->typeSpec());
                    out[safeText(fc->IDENTIFIER())] = {tname, 0};
                }
                collectLocalsFromBlock(fc->block(), beforeLine, out, flc);
            }
        }
        if (auto* ws = stmt->whileStmt())
            collectLocalsFromBlock(ws->block(), beforeLine, out, flc);
        if (auto* dw = stmt->doWhileStmt())
            collectLocalsFromBlock(dw->block(), beforeLine, out, flc);
        if (auto* ls = stmt->loopStmt())
            collectLocalsFromBlock(ls->block(), beforeLine, out, flc);
        if (auto* sw = stmt->switchStmt()) {
            for (auto* c : sw->caseClause())
                collectLocalsFromBlock(c->block(), beforeLine, out, flc);
            if (sw->defaultClause())
                collectLocalsFromBlock(sw->defaultClause()->block(), beforeLine, out, flc);
        }
        if (auto* tc = stmt->tryCatchStmt()) {
            collectLocalsFromBlock(tc->block(), beforeLine, out, flc);
            for (auto* cc : tc->catchClause()) {
                if (cc->IDENTIFIER() && cc->typeSpec())
                    out[safeText(cc->IDENTIFIER())] = {safeText(cc->typeSpec()), 0};
                collectLocalsFromBlock(cc->block(), beforeLine, out, flc);
            }
            if (tc->finallyClause())
                collectLocalsFromBlock(tc->finallyClause()->block(), beforeLine, out, flc);
        }
        // Structural blocks — recurse into body to collect visible locals
        if (auto* nb = stmt->nakedBlockStmt())
            collectLocalsFromStmts(nb->statement(), beforeLine, out, flc);
        if (auto* ib = stmt->inlineBlockStmt())
            collectLocalsFromStmts(ib->statement(), beforeLine, out, flc);
        if (auto* sb = stmt->scopeBlockStmt())
            collectLocalsFromStmts(sb->statement(), beforeLine, out, flc);
    }
}

static void collectLocalsFromBlock(
        LuxParser::BlockContext* block,
        size_t beforeLine,
        std::unordered_map<std::string, HoverProvider::LocalVar>& out,
        const FuncLookupCtx* flc) {
    if (!block) return;
    collectLocalsFromStmts(block->statement(), beforeLine, out, flc);
}

std::unordered_map<std::string, HoverProvider::LocalVar>
HoverProvider::collectLocals(LuxParser::FunctionDeclContext* func,
                             size_t beforeLine,
                             LuxParser::ProgramContext* tree,
                             const CBindings* bindings,
                             const ProjectContext* project) {
    std::unordered_map<std::string, LocalVar> result;

    // Build lookup context for resolving function call return types
    FuncLookupCtx flc;
    flc.tree       = tree;
    flc.bindings   = bindings;
    flc.builtinReg = &builtinRegistry_;
        flc.intrinsicReg = &intrinsicRegistry_;
    flc.extTypeReg = &extTypeRegistry_;
    flc.methodReg  = &methodRegistry_;
    flc.project    = project;

    // Collect parameters
    if (auto* params = func->paramList()) {
        for (auto* p : params->param()) {
            if (!p->typeSpec() || !p->IDENTIFIER()) continue;
            std::string typeName = safeText(p->typeSpec());
            std::string paramName = safeText(p->IDENTIFIER());
            result[paramName] = {typeName, 0, true};
        }
    }

    // Collect locals from function body
    collectLocalsFromBlock(func->block(), beforeLine, result, &flc);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
//  AST lookup helpers
// ═══════════════════════════════════════════════════════════════════════

LuxParser::FunctionDeclContext*
HoverProvider::findEnclosingFunction(LuxParser::ProgramContext* tree,
                                     size_t line) {
    size_t tokenLine = line + 1; // convert to 1-based
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

LuxParser::FunctionDeclContext*
HoverProvider::findFunctionDecl(LuxParser::ProgramContext* tree,
                                const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* func = tld->functionDecl()) {
            if (safeIdAt(func, 0) == name)
                return func;
        }
    }
    return nullptr;
}

LuxParser::StructDeclContext*
HoverProvider::findStructDecl(LuxParser::ProgramContext* tree,
                              const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* sd = tld->structDecl()) {
            if (safeText(sd->IDENTIFIER()) == name)
                return sd;
        }
    }
    return nullptr;
}

LuxParser::EnumDeclContext*
HoverProvider::findEnumDecl(LuxParser::ProgramContext* tree,
                            const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* ed = tld->enumDecl()) {
            if (ed->IDENTIFIER() && safeText(ed->IDENTIFIER()) == name)
                return ed;
        }
    }
    return nullptr;
}

LuxParser::UnionDeclContext*
HoverProvider::findUnionDecl(LuxParser::ProgramContext* tree,
                             const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* ud = tld->unionDecl()) {
            if (safeText(ud->IDENTIFIER()) == name)
                return ud;
        }
    }
    return nullptr;
}

LuxParser::TypeAliasDeclContext*
HoverProvider::findTypeAliasDecl(LuxParser::ProgramContext* tree,
                                 const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* ta = tld->typeAliasDecl()) {
            if (safeText(ta->IDENTIFIER()) == name)
                return ta;
        }
    }
    return nullptr;
}

LuxParser::ExtendDeclContext*
HoverProvider::findExtendDecl(LuxParser::ProgramContext* tree,
                              const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* ext = tld->extendDecl()) {
            if (safeText(ext->IDENTIFIER()) == name)
                return ext;
        }
    }
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
//  Formatting helpers
// ═══════════════════════════════════════════════════════════════════════

std::string HoverProvider::typeSpecToString(LuxParser::TypeSpecContext* ctx) {
    if (!ctx) return "?";
    // Use the raw text from the parse tree — preserves the original syntax.
    return ctx->getText();
}

std::string HoverProvider::formatFunctionDecl(LuxParser::FunctionDeclContext* func) {
    std::ostringstream ss;
    ss << "```lux\n";
    ss << "fn " << safeIdAt(func, 0) << "(";
    if (auto* params = func->paramList()) {
        bool first = true;
        for (auto* p : params->param()) {
            if (!first) ss << ", ";
            first = false;
            ss << typeSpecToString(p->typeSpec());
            if (p->SPREAD()) ss << " ...";
            else ss << " ";
            ss << safeText(p->IDENTIFIER());
        }
    }
    ss << ") " << typeSpecToString(func->typeSpec()) << "\n```";
    return ss.str();
}

std::string HoverProvider::formatExternDecl(LuxParser::ExternDeclContext* ext) {
    std::ostringstream ss;
    ss << "```lux\nextern " << typeSpecToString(ext->typeSpec())
       << " " << safeText(ext->IDENTIFIER()) << "(";
    if (auto* params = ext->externParamList()) {
        bool first = true;
        for (auto* ep : params->externParam()) {
            if (!first) ss << ", ";
            first = false;
            ss << typeSpecToString(ep->typeSpec());
            if (ep->IDENTIFIER())
                ss << " " << safeText(ep->IDENTIFIER());
        }
    }
    if (ext->SPREAD()) {
        if (ext->externParamList()) ss << ", ";
        ss << "...";
    }
    ss << ")\n```";
    return ss.str();
}

std::string HoverProvider::formatStructDecl(LuxParser::StructDeclContext* decl) {
    std::ostringstream ss;
    ss << "```lux\nstruct " << safeText(decl->IDENTIFIER());
    if (auto* tpl = decl->typeParamList()) {
        ss << "<";
        bool first = true;
        for (auto* tp : tpl->typeParam()) {
            if (!first) ss << ", ";
            auto ids = tp->IDENTIFIER();
            if (!ids.empty()) ss << ids[0]->getText();
            if (tp->COLON() && ids.size() >= 2) ss << ": " << ids[1]->getText();
            first = false;
        }
        ss << ">";
    }
    ss << " {\n";
    for (auto* f : decl->structField()) {
        ss << "    " << typeSpecToString(f->typeSpec()) << " "
           << safeText(f->IDENTIFIER()) << "\n";
    }
    ss << "}\n```";

    // Show extend methods if available
    // (We don't have access to tree here, but the caller can append if needed)
    return ss.str();
}

std::string HoverProvider::formatEnumDecl(LuxParser::EnumDeclContext* decl) {
    std::ostringstream ss;
    ss << "```lux\nenum " << safeText(decl->IDENTIFIER());
    if (auto* tpl = decl->typeParamList()) {
        ss << "<";
        bool first = true;
        for (auto* tp : tpl->typeParam()) {
            if (!first) ss << ", ";
            auto ids = tp->IDENTIFIER();
            if (!ids.empty()) ss << ids[0]->getText();
            if (tp->COLON() && ids.size() >= 2) ss << ": " << ids[1]->getText();
            first = false;
        }
        ss << ">";
    }
    ss << " {\n";
    auto variants = decl->enumVariant();
    for (size_t i = 0; i < variants.size(); i++) {
        auto* v = variants[i];
        ss << "    " << safeText(v->IDENTIFIER());
        if (v->LPAREN()) {
            ss << "(";
            auto tys = v->typeSpec();
            for (size_t j = 0; j < tys.size(); j++) {
                if (j) ss << ", ";
                ss << typeSpecToString(tys[j]);
            }
            ss << ")";
        } else if (v->LBRACE()) {
            ss << " { ";
            auto fields = v->enumPayloadField();
            for (size_t j = 0; j < fields.size(); j++) {
                if (j) ss << ", ";
                ss << safeText(fields[j]->IDENTIFIER()) << ": "
                   << typeSpecToString(fields[j]->typeSpec());
            }
            ss << " }";
        }
        if (i + 1 < variants.size()) ss << ",";
        ss << "\n";
    }
    ss << "}\n```";
    return ss.str();
}

std::string HoverProvider::formatUnionDecl(LuxParser::UnionDeclContext* decl) {
    std::ostringstream ss;
    ss << "```lux\nunion " << safeText(decl->IDENTIFIER());
    if (auto* tpl = decl->typeParamList()) {
        ss << "<";
        bool first = true;
        for (auto* tp : tpl->typeParam()) {
            if (!first) ss << ", ";
            auto ids = tp->IDENTIFIER();
            if (!ids.empty()) ss << ids[0]->getText();
            if (tp->COLON() && ids.size() >= 2) ss << ": " << ids[1]->getText();
            first = false;
        }
        ss << ">";
    }
    ss << " {\n";
    for (auto* f : decl->unionField()) {
        ss << "    " << typeSpecToString(f->typeSpec()) << " "
           << safeText(f->IDENTIFIER()) << "\n";
    }
    ss << "}\n```";
    return ss.str();
}

std::string HoverProvider::formatCFunction(const CFunction& func) {
    std::ostringstream ss;
    ss << "```c\n" << func.returnType->name << " " << func.name << "(";
    for (size_t i = 0; i < func.paramTypes.size(); i++) {
        if (i > 0) ss << ", ";
        ss << func.paramTypes[i]->name;
        // Show parameter name if available and non-empty
        if (i < func.paramNames.size() && !func.paramNames[i].empty())
            ss << " " << func.paramNames[i];
    }
    if (func.isVariadic) {
        if (!func.paramTypes.empty()) ss << ", ";
        ss << "...";
    }
    ss << ")\n```";
    if (!func.doc.empty())
        ss << "\n" << func.doc;
    return ss.str();
}

std::string HoverProvider::formatCStruct(const std::string& name,
                                          const CStruct& s) {
    std::ostringstream ss;
    ss << "```c\nstruct " << name << " {\n";
    for (auto& f : s.fields) {
        ss << "    " << f.typeInfo->name << " " << f.name << "\n";
    }
    ss << "}\n```\n*C struct*";
    return ss.str();
}

std::string HoverProvider::formatCEnum(const std::string& name,
                                        const CEnum& e) {
    std::ostringstream ss;
    ss << "```c\nenum " << name << " {\n";
    size_t count = 0;
    for (auto& [vname, val] : e.values) {
        ss << "    " << vname << " = " << val;
        if (count + 1 < e.values.size()) ss << ",";
        ss << "\n";
        count++;
        // Limit to 20 variants for large enums
        if (count >= 20 && e.values.size() > 20) {
            ss << "    // ... " << (e.values.size() - 20) << " more\n";
            break;
        }
    }
    ss << "}\n```\n*C enum*";
    return ss.str();
}

std::string HoverProvider::formatBuiltinSignature(const BuiltinSignature& sig) {
    std::ostringstream ss;
    ss << "```lux\n(builtin) " << sig.returnType << " " << sig.name << "(";
    for (size_t i = 0; i < sig.paramTypes.size(); i++) {
        if (i > 0) ss << ", ";
        ss << sig.paramTypes[i];
        bool consumes = false;
        bool borrows = false;
        for (size_t idx : sig.consumingArgs) if (idx == i) consumes = true;
        for (size_t idx : sig.borrowedArgs) if (idx == i) borrows = true;
        if (consumes) ss << " [consumes]";
        else if (borrows) ss << " [borrow]";
    }
    if (sig.isVariadic) {
        if (!sig.paramTypes.empty()) ss << ", ";
        ss << "...";
    }
    ss << ")\n```";
    if (sig.returnsOwned) {
        ss << "\n\nReturns owned memory.";
    }
    if (!sig.description.empty()) {
        ss << "\n\n" << sig.description;
    }
    return ss.str();
}

std::string HoverProvider::formatTypeInfo(const TypeInfo* ti) {
    if (!ti) return "?";
    return ti->name;
}

std::string HoverProvider::formatExtendMethods(LuxParser::ExtendDeclContext* ext) {
    std::ostringstream ss;
    ss << "```lux\nextend " << safeText(ext->IDENTIFIER()) << " {\n";
    for (auto* m : ext->extendMethod()) {
        ss << "    fn " << safeIdAt(m, 0) << "(";

        // Check if first param is &self
        if (m->AMPERSAND()) {
            ss << "&self";
            if (auto* params = m->paramList()) {
                for (auto* p : params->param()) {
                    ss << ", " << typeSpecToString(p->typeSpec())
                       << " " << safeText(p->IDENTIFIER());
                }
            }
        } else {
            // Static method
            if (auto* params = m->paramList()) {
                bool first = true;
                for (auto* p : params->param()) {
                    if (!first) ss << ", ";
                    first = false;
                    ss << typeSpecToString(p->typeSpec())
                       << " " << safeText(p->IDENTIFIER());
                }
            }
        }
        ss << ") " << typeSpecToString(m->typeSpec()) << "\n";
    }
    ss << "}\n```";
    return ss.str();
}

HoverResult HoverProvider::makeResult(antlr4::Token* token,
                                       const std::string& contents) {
    HoverResult r;
    r.contents = normalizeHoverMarkdown(contents);
    r.line     = token->getLine() - 1; // to 0-based
    r.col      = token->getCharPositionInLine();
    r.endLine  = r.line;
    r.endCol   = r.col + token->getText().size();
    return r;
}
