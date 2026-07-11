#include "lsp/DefinitionProvider.h"
#include "lsp/ProjectContext.h"
#include "parser/Parser.h"
#include "ffi/CHeaderResolver.h"
#include "namespace/ModuleRegistry.h"

#include <filesystem>

namespace fs = std::filesystem;

// ═══════════════════════════════════════════════════════════════════════
//  Static helpers
// ═══════════════════════════════════════════════════════════════════════

static bool containsToken(antlr4::ParserRuleContext* ctx, antlr4::Token* token) {
    if (!ctx || !token) return false;
    auto* start = ctx->getStart();
    auto* stop  = ctx->getStop();
    if (!start || !stop) return false;
    size_t idx = token->getTokenIndex();
    return idx >= start->getTokenIndex() && idx <= stop->getTokenIndex();
}

static std::string safeText(antlr4::tree::TerminalNode *n) {
    return n ? n->getText() : "";
}
static std::string safeText(antlr4::ParserRuleContext *ctx) {
    return ctx ? ctx->getText() : "";
}
static std::string extractBaseTypeName(LucisParser::TypeSpecContext* typeSpec) {
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

// Forward declarations for local collectors
static void collectLocalsFromBlock(
    LucisParser::BlockContext* block, size_t beforeLine,
    std::unordered_map<std::string, DefinitionProvider::LocalVar>& out);

static void collectLocalsFromStmts(
    const std::vector<LucisParser::StatementContext*>& stmts, size_t beforeLine,
    std::unordered_map<std::string, DefinitionProvider::LocalVar>& out) {

    auto cursorInsideNode = [](antlr4::ParserRuleContext* node, size_t cursorLine0) {
        if (!node) return false;
        auto* start = node->getStart();
        auto* stop  = node->getStop();
        if (!start || !stop) return false;
        size_t startLine = start->getLine() - 1;
        size_t stopLine  = stop->getLine() - 1;
        return cursorLine0 >= startLine && cursorLine0 <= stopLine;
    };

    for (auto* stmt : stmts) {
        auto* start = stmt->getStart();
        if (start && start->getLine() > beforeLine + 1) break;

        if (auto* vd = stmt->varDeclStmt()) {
            std::string typeName;
            if (vd->typeSpec()) typeName = safeText(vd->typeSpec());
            if (vd->LPAREN()) {
                for (auto* id : vd->IDENTIFIER()) {
                    std::string varName = id->getText();
                    out[varName] = {typeName, 0, id->getSymbol()};
                }
            } else {
                for (auto* d : vd->varDeclarator()) {
                    std::string varName = safeText(d->IDENTIFIER());
                    out[varName] = {typeName, 0, d->IDENTIFIER()->getSymbol()};
                }
            }

            if (auto* cu = dynamic_cast<LucisParser::CatchUnwrapExprContext*>(vd->expression())) {
                if (cursorInsideNode(cu->block(), beforeLine)) {
                    out["it"] = {"Error", 0, cu->CATCH()->getSymbol()};
                    collectLocalsFromBlock(cu->block(), beforeLine, out);
                }
            }
        }

        // Const declarations
        if (auto* cd = stmt->constDeclStmt()) {
            for (auto* d : cd->constDeclarator()) {
                std::string constName = safeText(d->IDENTIFIER());
                if (!constName.empty()) {
                    std::string typeName;
                    if (d->typeSpec()) typeName = safeText(d->typeSpec());
                    out[constName] = {typeName, 0, d->IDENTIFIER()->getSymbol()};
                }
            }
        }

        // Recurse into nested blocks
        if (auto* ifS = stmt->ifStmt()) {
            if (auto* body = ifS->ifBody()) {
                if (auto* b = body->block()) {
                    if (cursorInsideNode(b, beforeLine)) {
                        if (auto* isE = dynamic_cast<LucisParser::IsExprContext*>(ifS->expression());
                            isE && isE->IDENTIFIER(1)) {
                            auto* bindId = isE->IDENTIFIER(1);
                            out[bindId->getText()] = {"auto", 0, bindId->getSymbol()};
                        }
                        collectLocalsFromBlock(b, beforeLine, out);
                    }
                } else if (auto* s = body->statement()) {
                    if (cursorInsideNode(s, beforeLine)) {
                        if (auto* isE = dynamic_cast<LucisParser::IsExprContext*>(ifS->expression());
                            isE && isE->IDENTIFIER(1)) {
                            auto* bindId = isE->IDENTIFIER(1);
                            out[bindId->getText()] = {"auto", 0, bindId->getSymbol()};
                        }
                        collectLocalsFromStmts({s}, beforeLine, out);
                    }
                }
            }
            for (auto* elif : ifS->elseIfClause()) {
                if (auto* body = elif->ifBody()) {
                    if (auto* b = body->block()) {
                        if (cursorInsideNode(b, beforeLine)) {
                            if (auto* isE = dynamic_cast<LucisParser::IsExprContext*>(elif->expression());
                                isE && isE->IDENTIFIER(1)) {
                                auto* bindId = isE->IDENTIFIER(1);
                                out[bindId->getText()] = {"auto", 0, bindId->getSymbol()};
                            }
                            collectLocalsFromBlock(b, beforeLine, out);
                        }
                    } else if (auto* s = body->statement()) {
                        if (cursorInsideNode(s, beforeLine)) {
                            if (auto* isE = dynamic_cast<LucisParser::IsExprContext*>(elif->expression());
                                isE && isE->IDENTIFIER(1)) {
                                auto* bindId = isE->IDENTIFIER(1);
                                out[bindId->getText()] = {"auto", 0, bindId->getSymbol()};
                            }
                            collectLocalsFromStmts({s}, beforeLine, out);
                        }
                    }
                }
            }
            if (ifS->elseClause()) {
                if (auto* body = ifS->elseClause()->ifBody()) {
                    if (auto* b = body->block()) {
                        if (cursorInsideNode(b, beforeLine))
                            collectLocalsFromBlock(b, beforeLine, out);
                    } else if (auto* s = body->statement()) {
                        if (cursorInsideNode(s, beforeLine))
                            collectLocalsFromStmts({s}, beforeLine, out);
                    }
                }
            }
        }
        if (auto* forIn = stmt->forStmt()) {
            if (auto* fin = dynamic_cast<LucisParser::ForInStmtContext*>(forIn)) {
                if (fin->typeSpec() && fin->IDENTIFIER()) {
                    std::string tname = safeText(fin->typeSpec());
                    out[safeText(fin->IDENTIFIER())] =
                        {tname, 0, fin->IDENTIFIER()->getSymbol()};
                }
                collectLocalsFromBlock(fin->block(), beforeLine, out);
            }
            if (auto* fc = dynamic_cast<LucisParser::ForClassicStmtContext*>(forIn)) {
                if (fc->typeSpec() && fc->IDENTIFIER()) {
                    std::string tname = safeText(fc->typeSpec());
                    out[safeText(fc->IDENTIFIER())] =
                        {tname, 0, fc->IDENTIFIER()->getSymbol()};
                }
                collectLocalsFromBlock(fc->block(), beforeLine, out);
            }
        }
        if (auto* ws = stmt->whileStmt())
            collectLocalsFromBlock(ws->block(), beforeLine, out);
        if (auto* dw = stmt->doWhileStmt())
            collectLocalsFromBlock(dw->block(), beforeLine, out);
        if (auto* ls = stmt->loopStmt())
            collectLocalsFromBlock(ls->block(), beforeLine, out);
        if (auto* sw = stmt->switchStmt()) {
            for (auto* c : sw->caseClause())
                collectLocalsFromBlock(c->block(), beforeLine, out);
            if (sw->defaultClause())
                collectLocalsFromBlock(sw->defaultClause()->block(), beforeLine, out);
        }
        if (auto* tc = stmt->tryCatchStmt()) {
            collectLocalsFromBlock(tc->block(), beforeLine, out);
            for (auto* cc : tc->catchClause()) {
                if (cc->IDENTIFIER() && cc->typeSpec()) {
                    out[safeText(cc->IDENTIFIER())] =
                        {safeText(cc->typeSpec()), 0, cc->IDENTIFIER()->getSymbol()};
                }
                collectLocalsFromBlock(cc->block(), beforeLine, out);
            }
            if (tc->finallyClause())
                collectLocalsFromBlock(tc->finallyClause()->block(), beforeLine, out);
        }
    }
}

static void collectLocalsFromBlock(
    LucisParser::BlockContext* block, size_t beforeLine,
    std::unordered_map<std::string, DefinitionProvider::LocalVar>& out) {
    if (!block) return;
    collectLocalsFromStmts(block->statement(), beforeLine, out);
}

// ═══════════════════════════════════════════════════════════════════════
//  Main entry point
// ═══════════════════════════════════════════════════════════════════════

std::optional<DefinitionResult> DefinitionProvider::definition(
    const std::string& source, size_t line, size_t col,
    const std::string& filePath, const ProjectContext* project,
    ParseResult* preParsed) {

    ParseResult localParseStorage;
    auto& parsed = preParsed ? *preParsed : (localParseStorage = Parser::parseString(source), localParseStorage);
    if (!parsed.tree) return std::nullopt;

    // Resolve C headers
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

    // Find token at cursor (0-based → 1-based)
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

    std::string tokenText = hoveredToken->getText();

    return resolveAtPosition(parsed.tree, hoveredToken, tokenText,
                             *cBindingsPtr, line, filePath, project);
}

// ═══════════════════════════════════════════════════════════════════════
//  Top-level resolution
// ═══════════════════════════════════════════════════════════════════════

std::optional<DefinitionResult> DefinitionProvider::resolveAtPosition(
    LucisParser::ProgramContext* tree, antlr4::Token* hoveredToken,
    const std::string& tokenText, const CBindings& bindings,
    size_t cursorLine, const std::string& filePath,
    const ProjectContext* project) {

    size_t tokenLine = cursorLine + 1; // 0-based → 1-based

    // ── #include directives: jump to the actual header file ─────────────
    {
        auto tokType = static_cast<size_t>(hoveredToken->getType());
        if (tokType == LucisParser::INCLUDE_SYS || tokType == LucisParser::INCLUDE_LOCAL) {
            std::string headerName = (tokType == LucisParser::INCLUDE_SYS)
                ? CHeaderResolver::extractSystemHeader(tokenText)
                : CHeaderResolver::extractLocalHeader(tokenText);
            if (!headerName.empty()) {
                std::string path = bindings.resolveHeaderPath(headerName);
                if (!path.empty()) {
                    DefinitionResult r;
                    r.uri     = "file://" + path;
                    r.line    = 0;
                    r.col     = 0;
                    r.endLine = 0;
                    r.endCol  = 0;
                    return r;
                }
            }
            return std::nullopt;
        }
    }

    // ── Use declarations: go to definition of the imported symbol ────
    for (auto* pre : tree->preambleDecl()) {
        auto* useDecl = pre->useDecl();
        if (!useDecl) continue;
        if (auto* root = dynamic_cast<LucisParser::UseRootContext*>(useDecl)) {
            if (root->IDENTIFIER() && root->IDENTIFIER()->getSymbol() == hoveredToken) {
                return std::nullopt;
            }
        }
        if (auto* item = dynamic_cast<LucisParser::UseItemContext*>(useDecl)) {
            if (!item->IDENTIFIER() || !item->modulePath()) continue;
            if (item->IDENTIFIER()->getSymbol() == hoveredToken) {
                std::string modulePath;
                for (auto* id : item->modulePath()->IDENTIFIER()) {
                    if (!modulePath.empty()) modulePath += "::";
                    modulePath += id->getText();
                }
                return resolveImportedSymbol(safeText(item->IDENTIFIER()),
                                             modulePath, project);
            }
        }
        if (auto* group = dynamic_cast<LucisParser::UseGroupContext*>(useDecl)) {
            if (!group->modulePath()) continue;
            std::string modulePath;
            for (auto* id : group->modulePath()->IDENTIFIER()) {
                if (!modulePath.empty()) modulePath += "::";
                modulePath += id->getText();
            }
            for (auto* id : group->IDENTIFIER()) {
                if (id->getSymbol() == hoveredToken) {
                    return resolveImportedSymbol(id->getText(), modulePath, project);
                }
            }
        }
    }

    // ── Top-level declarations: jump to self (declaration definition) ──
    for (auto* tld : tree->topLevelDecl()) {
        // Function name → jump to self
        if (auto* func = tld->functionDecl()) {
            if (!func->IDENTIFIER().empty() && func->IDENTIFIER(0)->getSymbol() == hoveredToken)
                return makeResult(hoveredToken, filePath);
            // Parameter type spec → resolve type name
            if (auto* params = func->paramList()) {
                for (auto* p : params->param()) {
                    if (auto r = resolveTypeSpecToken(p->typeSpec(), hoveredToken,
                            tree, bindings, filePath, project))
                        return r;
                }
            }
            // Return type spec
            if (auto r = resolveTypeSpecToken(func->typeSpec(), hoveredToken,
                    tree, bindings, filePath, project))
                return r;
        }

        // Struct name
        if (auto* sd = tld->structDecl()) {
            if (sd->IDENTIFIER() && sd->IDENTIFIER()->getSymbol() == hoveredToken)
                return makeResult(hoveredToken, filePath);
            // Field type specs
            for (auto* f : sd->structField()) {
                if (auto r = resolveTypeSpecToken(f->typeSpec(), hoveredToken,
                        tree, bindings, filePath, project))
                    return r;
            }
        }

        // Enum name
        if (auto* ed = tld->enumDecl()) {
            if (ed->IDENTIFIER() && ed->IDENTIFIER()->getSymbol() == hoveredToken)
                return makeResult(hoveredToken, filePath);
        }

        // Union name
        if (auto* ud = tld->unionDecl()) {
            if (ud->IDENTIFIER() && ud->IDENTIFIER()->getSymbol() == hoveredToken)
                return makeResult(hoveredToken, filePath);
            for (auto* f : ud->unionField()) {
                if (auto r = resolveTypeSpecToken(f->typeSpec(), hoveredToken,
                        tree, bindings, filePath, project))
                    return r;
            }
        }

        // Extend declaration
        if (auto* ext = tld->extendDecl()) {
            if (ext->IDENTIFIER() && ext->IDENTIFIER()->getSymbol() == hoveredToken) {
                // Jump to the struct definition
                return resolveTypeName(tokenText, tree, bindings, filePath, project);
            }
        }

        // Type alias
        if (auto* ta = tld->typeAliasDecl()) {
            if (ta->IDENTIFIER() && ta->IDENTIFIER()->getSymbol() == hoveredToken)
                return makeResult(hoveredToken, filePath);
            if (auto r = resolveTypeSpecToken(ta->typeSpec(), hoveredToken,
                    tree, bindings, filePath, project))
                return r;
        }

        // Extern declaration
        if (auto* ext = tld->externDecl()) {
            if (ext->IDENTIFIER() && ext->IDENTIFIER()->getSymbol() == hoveredToken)
                return makeResult(hoveredToken, filePath);
        }
    }

    // ── Walk function bodies ────────────────────────────────────────
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* func = tld->functionDecl()) {
            auto* startTok = func->getStart();
            auto* stopTok  = func->getStop();
            if (startTok && stopTok &&
                tokenLine >= startTok->getLine() && tokenLine <= stopTok->getLine()) {
                auto result = walkBlockForDef(func->block(), hoveredToken,
                    tokenText, tree, bindings, cursorLine, filePath, project);
                if (result) return result;
            }
            continue;
        }

        // Walk extend block method bodies
        if (auto* ext = tld->extendDecl()) {
            auto* startTok = ext->getStart();
            auto* stopTok  = ext->getStop();
            if (!startTok || !stopTok) continue;
            if (tokenLine < startTok->getLine() || tokenLine > stopTok->getLine())
                continue;

            for (auto* method : ext->extendMethod()) {
                auto* mStart = method->getStart();
                auto* mStop  = method->getStop();
                if (!mStart || !mStop) continue;
                if (tokenLine < mStart->getLine() || tokenLine > mStop->getLine())
                    continue;

                // Return type spec
                if (auto r = resolveTypeSpecToken(method->typeSpec(), hoveredToken,
                        tree, bindings, filePath, project))
                    return r;

                // Parameters type specs
                bool isStatic = (method->AMPERSAND() == nullptr);
                std::vector<LucisParser::ParamContext*> params;
                if (isStatic) {
                    if (auto* pl = method->paramList())
                        params = pl->param();
                } else {
                    params = method->param();
                }
                for (auto* p : params) {
                    if (auto r = resolveTypeSpecToken(p->typeSpec(), hoveredToken,
                            tree, bindings, filePath, project))
                        return r;
                }

                // Walk method body
                auto result = walkBlockForDef(method->block(), hoveredToken,
                    tokenText, tree, bindings, cursorLine, filePath, project);
                if (result) return result;
            }
        }
    }

    // ── Fallback: resolve any identifier ────────────────────────────
    if (hoveredToken->getType() == LucisLexer::IDENTIFIER) {
        return resolveIdent(tokenText, tree, bindings, cursorLine, filePath, project);
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  Resolve identifier to its definition
// ═══════════════════════════════════════════════════════════════════════

std::optional<DefinitionResult> DefinitionProvider::resolveIdent(
    const std::string& name, LucisParser::ProgramContext* tree,
    const CBindings& bindings, size_t cursorLine,
    const std::string& filePath, const ProjectContext* project) {

    // 1) Local variable / parameter
    auto* func = findEnclosingFunction(tree, cursorLine);
    if (func) {
        auto locals = collectLocals(func, cursorLine);
        auto it = locals.find(name);
        if (it != locals.end() && it->second.declToken)
            return makeResult(it->second.declToken, filePath);
    }

    // 1.5) Parameter or local inside extend method body
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
                std::vector<LucisParser::ParamContext*> params;
                if (isStatic) {
                    if (auto* pl = method->paramList())
                        params = pl->param();
                } else {
                    params = method->param();
                }
                for (auto* p : params) {
                    if (p->IDENTIFIER() && safeText(p->IDENTIFIER()) == name)
                        return makeResult(p->IDENTIFIER()->getSymbol(), filePath);
                }

                // Locals from method body
                std::unordered_map<std::string, LocalVar> locals;
                collectLocalsFromBlock(method->block(), cursorLine, locals);
                auto it = locals.find(name);
                if (it != locals.end() && it->second.declToken)
                    return makeResult(it->second.declToken, filePath);
            }
        }
    }

    // 2) User-defined function
    auto* funcDecl = findFunctionDecl(tree, name);
    if (funcDecl)
        return makeResult(funcDecl->IDENTIFIER(0)->getSymbol(), filePath);

    // 3) Extern declaration
    auto* externDecl = findExternDecl(tree, name);
    if (externDecl)
        return makeResult(externDecl->IDENTIFIER()->getSymbol(), filePath);

    // 4) Struct
    auto* structDecl = findStructDecl(tree, name);
    if (structDecl)
        return makeResult(structDecl->IDENTIFIER()->getSymbol(), filePath);

    // 5) Enum
    auto* enumDecl = findEnumDecl(tree, name);
    if (enumDecl)
        return makeResult(enumDecl->IDENTIFIER()->getSymbol(), filePath);

    // 6) Union
    auto* unionDecl = findUnionDecl(tree, name);
    if (unionDecl)
        return makeResult(unionDecl->IDENTIFIER()->getSymbol(), filePath);

    // 7) Type alias
    auto* aliasDecl = findTypeAliasDecl(tree, name);
    if (aliasDecl)
        return makeResult(aliasDecl->IDENTIFIER()->getSymbol(), filePath);

    // 8) Extend block
    auto* extDecl = findExtendDecl(tree, name);
    if (extDecl)
        return makeResult(extDecl->IDENTIFIER()->getSymbol(), filePath);

    // 8.5) Top-level const declaration
    auto* constDecl = findConstDecl(tree, name);
    if (constDecl) {
        for (auto* d : constDecl->constDeclarator()) {
            if (d->IDENTIFIER() && safeText(d->IDENTIFIER()) == name)
                return makeResult(d->IDENTIFIER()->getSymbol(), filePath);
        }
    }

    // 9) Enum variant (Name inside EnumName::Name or standalone imported)
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* ed = tld->enumDecl()) {
            for (auto* variant : ed->enumVariant()) {
                auto* v = variant->IDENTIFIER();
                if (v && v->getText() == name)
                    return makeResult(v->getSymbol(), filePath);
            }
        }
    }

    // 9.5) Enum variant from `use EnumType::*;`
    auto tryEnumWildcardDef = [&](LucisParser::UseDeclContext* useDecl) -> std::optional<DefinitionResult> {
        auto* ew = dynamic_cast<LucisParser::UseEnumWildcardContext*>(useDecl);
        if (!ew) return std::nullopt;
        auto baseName = extractBaseTypeName(ew->typeSpec());
        if (baseName.empty()) return std::nullopt;
        auto* ed = findEnumDecl(tree, baseName);
        if (ed) {
            for (auto* variant : ed->enumVariant()) {
                auto* v = variant->IDENTIFIER();
                if (v && v->getText() == name)
                    return makeResult(v->getSymbol(), filePath);
            }
        }
        if (!ed && project && project->isValid()) {
            for (auto& ns : project->registry().allModules()) {
                auto* sym = project->registry().findSymbol(ns, baseName);
                if (!sym || sym->kind != ExportedSymbol::Enum) continue;
                auto* decl = dynamic_cast<LucisParser::EnumDeclContext*>(sym->decl);
                if (!decl) continue;
                for (auto* variant : decl->enumVariant()) {
                    auto* v = variant->IDENTIFIER();
                    if (v && v->getText() == name)
                        return makeResult(v->getSymbol(), filePath, sym->sourceFile);
                }
            }
        }
        return std::nullopt;
    };
    for (auto* pre : tree->preambleDecl()) {
        auto* useDecl = pre->useDecl();
        if (!useDecl) continue;
        auto r = tryEnumWildcardDef(useDecl);
        if (r) return r;
    }
    // Also check in-function use EnumType::*; statements before cursor
    {
        auto* enclosingFunc = findEnclosingFunction(tree, cursorLine);
        if (enclosingFunc && enclosingFunc->block()) {
            for (auto* stmt : enclosingFunc->block()->statement()) {
                auto* start = stmt->getStart();
                if (start && start->getLine() - 1 > cursorLine) break;
                if (auto* useDecl = stmt->useDecl()) {
                    auto r = tryEnumWildcardDef(useDecl);
                    if (r) return r;
                }
            }
        }
    }

    // 10) Cross-file symbol from project registry
    if (project && project->isValid()) {
        for (auto& ns : project->registry().allModules()) {
            auto* sym = project->registry().findSymbol(ns, name);
            if (!sym || !sym->decl) continue;
            // Phase 4: use ModuleRegistry location instead of AST token
            DefinitionResult r;
            r.uri     = "file://" + sym->sourceFile;
            r.line    = static_cast<int>(sym->line) - 1;
            r.col     = sym->column;
            r.endLine = r.line;
            r.endCol  = r.col + static_cast<unsigned>(sym->name.size());
            return r;
        }
    }

    // 11) C function
    if (auto* cf = bindings.findFunction(name)) {
        if (!cf->sourceFile.empty()) {
            DefinitionResult r;
            r.uri = "file://" + cf->sourceFile;
            r.line = r.endLine = cf->line;
            r.col = r.endCol = 0;
            return r;
        }
    }

    // 12) C macro constant
    if (auto* cm = bindings.findMacro(name)) {
        if (!cm->sourceFile.empty()) {
            DefinitionResult r;
            r.uri = "file://" + cm->sourceFile;
            r.line = r.endLine = cm->line;
            r.col = r.endCol = 0;
            return r;
        }
    }

    // 12b) C struct-literal macro
    if (auto* sm = bindings.findStructMacro(name)) {
        if (!sm->sourceFile.empty()) {
            DefinitionResult r;
            r.uri = "file://" + sm->sourceFile;
            r.line = r.endLine = sm->line;
            r.col = r.endCol = 0;
            return r;
        }
    }

    // 12c) C function-like macro
    if (auto* cflm = bindings.findFunctionLikeMacro(name)) {
        if (!cflm->sourceFile.empty()) {
            DefinitionResult r;
            r.uri = "file://" + cflm->sourceFile;
            r.line = r.endLine = cflm->line;
            r.col = r.endCol = 0;
            return r;
        }
    }

    // 13) C struct / union
    if (auto* cs = bindings.findStruct(name)) {
        if (!cs->sourceFile.empty()) {
            DefinitionResult r;
            r.uri = "file://" + cs->sourceFile;
            r.line = r.endLine = cs->line;
            r.col = r.endCol = 0;
            return r;
        }
    }

    // 14) C enum type
    if (auto* ce = bindings.findEnum(name)) {
        if (!ce->sourceFile.empty()) {
            DefinitionResult r;
            r.uri = "file://" + ce->sourceFile;
            r.line = r.endLine = ce->line;
            r.col = r.endCol = 0;
            return r;
        }
    }

    // 14b) C enum constant (e.g. FLAG_VSYNC_HINT inside ConfigFlags)
    for (auto& [ename, cenum] : bindings.enums()) {
        auto vit = cenum.valueLocs.find(name);
        if (vit != cenum.valueLocs.end()) {
            DefinitionResult r;
            r.uri = "file://" + vit->second.first;
            r.line = r.endLine = vit->second.second;
            r.col = r.endCol = 0;
            return r;
        }
    }

    // 15) C typedef
    if (auto* ct = bindings.findTypedef(name)) {
        if (!ct->sourceFile.empty()) {
            DefinitionResult r;
            r.uri = "file://" + ct->sourceFile;
            r.line = r.endLine = ct->line;
            r.col = r.endCol = 0;
            return r;
        }
    }

    // 16) C global variable
    if (auto* gv = bindings.findGlobal(name)) {
        if (!gv->sourceFile.empty()) {
            DefinitionResult r;
            r.uri = "file://" + gv->sourceFile;
            r.line = r.endLine = gv->line;
            r.col = r.endCol = 0;
            return r;
        }
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  Resolve type name to its definition
// ═══════════════════════════════════════════════════════════════════════

std::optional<DefinitionResult> DefinitionProvider::resolveTypeName(
    const std::string& name, LucisParser::ProgramContext* tree,
    const CBindings& bindings, const std::string& filePath,
    const ProjectContext* project) {

    // Same-file struct/enum/union/typealias
    if (auto* sd = findStructDecl(tree, name))
        return makeResult(sd->IDENTIFIER()->getSymbol(), filePath);
    if (auto* ed = findEnumDecl(tree, name))
        return makeResult(ed->IDENTIFIER()->getSymbol(), filePath);
    if (auto* ud = findUnionDecl(tree, name))
        return makeResult(ud->IDENTIFIER()->getSymbol(), filePath);
    if (auto* ta = findTypeAliasDecl(tree, name))
        return makeResult(ta->IDENTIFIER()->getSymbol(), filePath);

    // Cross-file
    if (project && project->isValid()) {
        for (auto& ns : project->registry().allModules()) {
            auto* sym = project->registry().findSymbol(ns, name);
            if (!sym || !sym->decl) continue;
            if (sym->kind != ExportedSymbol::Struct &&
                sym->kind != ExportedSymbol::Enum &&
                sym->kind != ExportedSymbol::Union &&
                sym->kind != ExportedSymbol::TypeAlias) continue;
            // Phase 4: use ModuleRegistry location
            DefinitionResult r;
            r.uri     = "file://" + sym->sourceFile;
            r.line    = static_cast<int>(sym->line) - 1;
            r.col     = sym->column;
            r.endLine = r.line;
            r.endCol  = r.col + static_cast<unsigned>(sym->name.size());
            return r;
        }
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  Resolve imported symbol from `use` declaration
// ═══════════════════════════════════════════════════════════════════════

std::optional<DefinitionResult> DefinitionProvider::resolveImportedSymbol(
    const std::string& symbolName, const std::string& modulePath,
    const ProjectContext* project) {

    if (!project || !project->isValid()) return std::nullopt;

    auto registryPath = ModuleRegistry::usePathToModulePath(modulePath);
    auto* sym = project->registry().findSymbol(registryPath, symbolName);
    if (!sym || !sym->decl) return std::nullopt;

    // Phase 4: use ModuleRegistry location
    DefinitionResult r;
    r.uri     = "file://" + sym->sourceFile;
    r.line    = static_cast<int>(sym->line) - 1;
    r.col     = sym->column;
    r.endLine = r.line;
    r.endCol  = r.col + static_cast<unsigned>(symbolName.size());
    return r;
}

// ═══════════════════════════════════════════════════════════════════════
//  Resolve type specs (hovering on a type name in a type annotation)
// ═══════════════════════════════════════════════════════════════════════

std::optional<DefinitionResult> DefinitionProvider::resolveTypeSpecToken(
    LucisParser::TypeSpecContext* typeSpec, antlr4::Token* hoveredToken,
    LucisParser::ProgramContext* tree, const CBindings& bindings,
    const std::string& filePath, const ProjectContext* project) {

    if (!typeSpec) return std::nullopt;

    // TypeSpec has a single IDENTIFIER for the type name
    if (auto* id = typeSpec->IDENTIFIER()) {
        if (id->getSymbol() == hoveredToken) {
            return resolveTypeName(id->getText(), tree, bindings, filePath, project);
        }
    }

    // Recurse into nested type specs (generics, arrays, pointers, etc.)
    for (auto* nested : typeSpec->typeSpec()) {
        if (auto r = resolveTypeSpecToken(nested, hoveredToken, tree, bindings,
                                          filePath, project))
            return r;
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  AST walker: expressions
// ═══════════════════════════════════════════════════════════════════════

std::optional<DefinitionResult> DefinitionProvider::walkExprForDef(
    LucisParser::ExpressionContext* expr, antlr4::Token* hoveredToken,
    const std::string& tokenText, LucisParser::ProgramContext* tree,
    const CBindings& bindings, size_t cursorLine,
    const std::string& filePath, const ProjectContext* project) {

    if (!expr || !containsToken(expr, hoveredToken)) return std::nullopt;
    if (auto* fc = dynamic_cast<LucisParser::FnCallExprContext*>(expr)) {
        if (auto r = walkExprForDef(fc->expression(), hoveredToken, tokenText,
                                    tree, bindings, cursorLine, filePath, project))
            return r;
        if (auto* args = fc->argList()) {
            for (auto* arg : args->expression()) {
                if (auto r = walkExprForDef(arg, hoveredToken, tokenText, tree,
                                            bindings, cursorLine, filePath, project))
                    return r;
            }
        }
        return std::nullopt;
    }

    // ── Method call: expr.method(args) ────────────────────────────
    if (auto* mc = dynamic_cast<LucisParser::MethodCallExprContext*>(expr)) {
        if (auto* id = mc->IDENTIFIER()) {
            if (id->getSymbol() == hoveredToken) {
                // Resolve method name to its extend method declaration
                std::string methodName = id->getText();
                auto* receiver = mc->expression();
                std::string receiverTypeName;

                // Try to get receiver variable type from locals
                if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(receiver)) {
                    auto* encFunc = findEnclosingFunction(tree, cursorLine);
                    if (encFunc) {
                        auto locals = collectLocals(encFunc, cursorLine);
                        auto it = locals.find(safeText(ident->IDENTIFIER()));
                        if (it != locals.end())
                            receiverTypeName = it->second.typeName;
                    }
                }

                // Search local extend blocks
                for (auto* tld : tree->topLevelDecl()) {
                    auto* ext = tld->extendDecl();
                    if (!ext) continue;
                    if (!receiverTypeName.empty() &&
                        safeText(ext->IDENTIFIER()) != receiverTypeName)
                        continue;
                    for (auto* m : ext->extendMethod()) {
                        if (safeIdAt(m, 0) == methodName)
                            return makeResult(m->IDENTIFIER(0)->getSymbol(), filePath);
                    }
                }

                // Search cross-file extend blocks
                if (project && project->isValid()) {
                    for (auto& ns : project->registry().allModules()) {
                        auto syms = project->registry().getModuleSymbols(ns);
                        for (auto* sym : syms) {
                            if (sym->kind != ExportedSymbol::ExtendBlock) continue;
                            auto* ext = dynamic_cast<LucisParser::ExtendDeclContext*>(sym->decl);
                            if (!ext) continue;
                            if (!receiverTypeName.empty() &&
                                safeText(ext->IDENTIFIER()) != receiverTypeName)
                                continue;
                            for (auto* m : ext->extendMethod()) {
                                if (safeIdAt(m, 0) == methodName)
                                    return makeResult(m->IDENTIFIER(0)->getSymbol(),
                                                      filePath, sym->sourceFile);
                            }
                        }
                    }
                }

                return std::nullopt;
            }
        }
        if (auto r = walkExprForDef(mc->expression(), hoveredToken, tokenText,
                                    tree, bindings, cursorLine, filePath, project))
            return r;
        if (auto* args = mc->argList()) {
            for (auto* arg : args->expression()) {
                if (auto r = walkExprForDef(arg, hoveredToken, tokenText, tree,
                                            bindings, cursorLine, filePath, project))
                    return r;
            }
        }
        return std::nullopt;
    }

    // ── Field access: expr.field ──────────────────────────────────
    if (auto* fa = dynamic_cast<LucisParser::FieldAccessExprContext*>(expr)) {
        // If hovering over the field name, resolve it to the struct field decl
        if (fa->IDENTIFIER()->getSymbol() == hoveredToken ||
            fa->IDENTIFIER()->getSymbol()->getTokenIndex() == hoveredToken->getTokenIndex()) {
            std::string structName = inferExprStructType(fa->expression(), tree,
                                                         cursorLine);

            if (structName.empty()) {
                if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(fa->expression())) {
                    if (auto* callee = dynamic_cast<LucisParser::IdentExprContext*>(call->expression())) {
                        std::string calleeName = safeText(callee->IDENTIFIER());

                        if (auto* cf = bindings.findFunction(calleeName)) {
                            if (cf->returnType) structName = cf->returnType->name;
                        }

                        if (structName.empty()) {
                            if (auto* fd = findFunctionDecl(tree, calleeName)) {
                                if (fd->typeSpec()) structName = safeText(fd->typeSpec());
                            }
                        }

                        if (structName.empty() && project && project->isValid()) {
                            for (auto& ns : project->registry().allModules()) {
                                auto* sym = project->registry().findSymbol(ns, calleeName);
                                if (!sym || sym->kind != ExportedSymbol::Function) continue;
                                auto* fd = dynamic_cast<LucisParser::FunctionDeclContext*>(sym->decl);
                                if (fd && fd->typeSpec()) {
                                    structName = safeText(fd->typeSpec());
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            while (!structName.empty() && structName.front() == '*')
                structName.erase(structName.begin());

            if (!structName.empty())
                return resolveStructField(structName, tokenText, tree, bindings,
                                          filePath, project);
            return std::nullopt;
        }
        return walkExprForDef(fa->expression(), hoveredToken, tokenText,
                              tree, bindings, cursorLine, filePath, project);
    }

    // ── Arrow access: expr->field ─────────────────────────────────
    if (auto* aa = dynamic_cast<LucisParser::ArrowAccessExprContext*>(expr)) {
        if (aa->IDENTIFIER()->getSymbol() == hoveredToken ||
            aa->IDENTIFIER()->getSymbol()->getTokenIndex() == hoveredToken->getTokenIndex()) {
            std::string structName = inferExprStructType(aa->expression(), tree,
                                                         cursorLine);

            if (structName.empty()) {
                if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(aa->expression())) {
                    if (auto* callee = dynamic_cast<LucisParser::IdentExprContext*>(call->expression())) {
                        auto* cf = bindings.findFunction(safeText(callee->IDENTIFIER()));
                        if (cf && cf->returnType) structName = cf->returnType->name;
                    }
                }
            }

            while (!structName.empty() && structName.front() == '*')
                structName.erase(structName.begin());

            if (!structName.empty())
                return resolveStructField(structName, tokenText, tree, bindings,
                                          filePath, project);
            return std::nullopt;
        }
        return walkExprForDef(aa->expression(), hoveredToken, tokenText,
                              tree, bindings, cursorLine, filePath, project);
    }

    // ── Enum access: EnumName::Variant ───────────────────────────
    if (auto* ea = dynamic_cast<LucisParser::EnumAccessExprContext*>(expr)) {
        auto ids = ea->IDENTIFIER();
        if (ids.size() >= 1 && ids[0]->getSymbol() == hoveredToken) {
            return resolveTypeName(ids[0]->getText(), tree, bindings,
                                   filePath, project);
        }
        if (ids.size() >= 2 && ids[1]->getSymbol() == hoveredToken) {
            // Jump to the enum variant
            std::string enumName = ids[0]->getText();
            auto* enumDecl = findEnumDecl(tree, enumName);
            if (enumDecl) {
                for (auto* variant : enumDecl->enumVariant()) {
                    auto* v = variant->IDENTIFIER();
                    if (v && v->getText() == ids[1]->getText())
                        return makeResult(v->getSymbol(), filePath);
                }
            }
            // Cross-file enum
            if (project && project->isValid()) {
                for (auto& ns : project->registry().allModules()) {
                    auto* sym = project->registry().findSymbol(ns, enumName);
                    if (!sym || sym->kind != ExportedSymbol::Enum) continue;
                    auto* ed = dynamic_cast<LucisParser::EnumDeclContext*>(sym->decl);
                    if (!ed) continue;
                    for (auto* variant : ed->enumVariant()) {
                        auto* v = variant->IDENTIFIER();
                        if (v && v->getText() == ids[1]->getText())
                            return makeResult(v->getSymbol(), filePath,
                                              sym->sourceFile);
                    }
                }
            }
        }
        return std::nullopt;
    }

    // ── Static method call: Type::method(args) ───────────────────
    if (auto* smc = dynamic_cast<LucisParser::StaticMethodCallExprContext*>(expr)) {
        auto ids = smc->IDENTIFIER();
        if (ids.size() >= 1 && ids[0]->getSymbol() == hoveredToken) {
            return resolveTypeName(ids[0]->getText(), tree, bindings,
                                   filePath, project);
        }
        // Static method name → resolve to extend method declaration
        if (ids.size() >= 2 && ids.back()->getSymbol() == hoveredToken) {
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

            // Search local extend blocks
            for (auto* tld : tree->topLevelDecl()) {
                auto* ext = tld->extendDecl();
                if (!ext || safeText(ext->IDENTIFIER()) != typeName) continue;
                for (auto* m : ext->extendMethod()) {
                    if (safeIdAt(m, 0) == methodName)
                        return makeResult(m->IDENTIFIER(0)->getSymbol(), filePath);
                }
            }

            // Search cross-file extend blocks
            if (project && project->isValid()) {
                for (auto& ns : project->registry().allModules()) {
                    auto syms = project->registry().getModuleSymbols(ns);
                    for (auto* sym : syms) {
                        if (sym->kind != ExportedSymbol::ExtendBlock) continue;
                        auto* ext = dynamic_cast<LucisParser::ExtendDeclContext*>(sym->decl);
                        if (!ext || safeText(ext->IDENTIFIER()) != typeName) continue;
                        for (auto* m : ext->extendMethod()) {
                            if (safeIdAt(m, 0) == methodName)
                                return makeResult(m->IDENTIFIER(0)->getSymbol(),
                                                  filePath, sym->sourceFile);
                        }
                    }
                }
            }
        }
        if (auto* args = smc->argList()) {
            for (auto* arg : args->expression()) {
                if (auto r = walkExprForDef(arg, hoveredToken, tokenText, tree,
                                            bindings, cursorLine, filePath, project))
                    return r;
            }
        }
        return std::nullopt;
    }

    // ── Struct literal: StructName { field: val, ... } ───────────
    if (auto* sl = dynamic_cast<LucisParser::StructLitExprContext*>(expr)) {
        auto ids = sl->IDENTIFIER();
        if (!ids.empty() && ids[0]->getSymbol() == hoveredToken) {
            return resolveTypeName(ids[0]->getText(), tree, bindings,
                                   filePath, project);
        }
        for (auto* subExpr : sl->expression()) {
            if (auto r = walkExprForDef(subExpr, hoveredToken, tokenText,
                                        tree, bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Cast expression: expr as Type ────────────────────────────
    if (auto* cast = dynamic_cast<LucisParser::CastExprContext*>(expr)) {
        if (auto r = walkExprForDef(cast->expression(), hoveredToken, tokenText,
                                    tree, bindings, cursorLine, filePath, project))
            return r;
        if (auto r = resolveTypeSpecToken(cast->typeSpec(), hoveredToken, tree,
                                          bindings, filePath, project))
            return r;
        return std::nullopt;
    }

    // ── Spawn expression: spawn funcCall ─────────────────────────
    if (auto* sp = dynamic_cast<LucisParser::SpawnExprContext*>(expr)) {
        return walkExprForDef(sp->expression(), hoveredToken, tokenText, tree,
                              bindings, cursorLine, filePath, project);
    }

    // ── Await expression: await expr ─────────────────────────────
    if (auto* aw = dynamic_cast<LucisParser::AwaitExprContext*>(expr)) {
        return walkExprForDef(aw->expression(), hoveredToken, tokenText, tree,
                              bindings, cursorLine, filePath, project);
    }

    // ── Try expression: try expr ─────────────────────────────────
    if (auto* te = dynamic_cast<LucisParser::TryExprContext*>(expr)) {
        return walkExprForDef(te->expression(0), hoveredToken, tokenText, tree,
                              bindings, cursorLine, filePath, project);
    }

    // ── Array literal ────────────────────────────────────────────
    if (auto* al = dynamic_cast<LucisParser::ArrayLitExprContext*>(expr)) {
        for (auto* elem : al->expression()) {
            if (auto r = walkExprForDef(elem, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Unary expressions (neg, logical not, bit not, deref, addr-of) ──
    if (auto* ne = dynamic_cast<LucisParser::NegExprContext*>(expr)) {
        return walkExprForDef(ne->expression(), hoveredToken, tokenText, tree,
                              bindings, cursorLine, filePath, project);
    }
    if (auto* ln = dynamic_cast<LucisParser::LogicalNotExprContext*>(expr)) {
        return walkExprForDef(ln->expression(), hoveredToken, tokenText, tree,
                              bindings, cursorLine, filePath, project);
    }
    if (auto* bn = dynamic_cast<LucisParser::BitNotExprContext*>(expr)) {
        return walkExprForDef(bn->expression(), hoveredToken, tokenText, tree,
                              bindings, cursorLine, filePath, project);
    }
    if (auto* dr = dynamic_cast<LucisParser::DerefExprContext*>(expr)) {
        return walkExprForDef(dr->expression(), hoveredToken, tokenText, tree,
                              bindings, cursorLine, filePath, project);
    }
    if (auto* ao = dynamic_cast<LucisParser::AddrOfExprContext*>(expr)) {
        // &expression — resolve the inner expression
        return walkExprForDef(ao->expression(), hoveredToken, tokenText, tree,
                              bindings, cursorLine, filePath, project);
    }

    // ── Binary expressions (arithmetic, comparison, logical, etc.) ──
    // AddSub, Mul, Shift, BitAnd, BitOr, BitXor
    if (auto* ae = dynamic_cast<LucisParser::AddSubExprContext*>(expr)) {
        for (auto* sub : ae->expression()) {
            if (auto r = walkExprForDef(sub, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }
    if (auto* me = dynamic_cast<LucisParser::MulExprContext*>(expr)) {
        for (auto* sub : me->expression()) {
            if (auto r = walkExprForDef(sub, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }
    // Relational and equality
    if (auto* re = dynamic_cast<LucisParser::RelExprContext*>(expr)) {
        for (auto* sub : re->expression()) {
            if (auto r = walkExprForDef(sub, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }
    if (auto* eq = dynamic_cast<LucisParser::EqExprContext*>(expr)) {
        for (auto* sub : eq->expression()) {
            if (auto r = walkExprForDef(sub, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }
    // Logical and/or
    if (auto* la = dynamic_cast<LucisParser::LogicalAndExprContext*>(expr)) {
        for (auto* sub : la->expression()) {
            if (auto r = walkExprForDef(sub, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }
    if (auto* lo = dynamic_cast<LucisParser::LogicalOrExprContext*>(expr)) {
        for (auto* sub : lo->expression()) {
            if (auto r = walkExprForDef(sub, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Index expression: expr[idx] ──────────────────────────────
    if (auto* ie = dynamic_cast<LucisParser::IndexExprContext*>(expr)) {
        for (auto* sub : ie->expression()) {
            if (auto r = walkExprForDef(sub, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // (No AssignExpr — assignment is a statement in Lucis)

    // ── Paren expression: (expr) ─────────────────────────────────
    if (auto* pe = dynamic_cast<LucisParser::ParenExprContext*>(expr)) {
        return walkExprForDef(pe->expression(), hoveredToken, tokenText, tree,
                              bindings, cursorLine, filePath, project);
    }

    // ── Ternary expression: cond ? then : else ───────────────────
    if (auto* te = dynamic_cast<LucisParser::TernaryExprContext*>(expr)) {
        for (auto* sub : te->expression()) {
            if (auto r = walkExprForDef(sub, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Null coalescing: expr ?? fallback ────────────────────────
    if (auto* nc = dynamic_cast<LucisParser::NullCoalExprContext*>(expr)) {
        for (auto* sub : nc->expression()) {
            if (auto r = walkExprForDef(sub, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Lambda expression: |params| expr ──────────────────────────
    if (auto* le = dynamic_cast<LucisParser::LambdaExprContext*>(expr)) {
        if (le->paramList()) {
            for (auto* child : le->paramList()->children) {
                if (auto* param = dynamic_cast<LucisParser::ParamContext*>(child)) {
                    if (param->IDENTIFIER() && param->IDENTIFIER()->getSymbol() == hoveredToken)
                        return resolveIdent(tokenText, tree, bindings, cursorLine,
                                            filePath, project);
                }
            }
        }
        if (le->expression()) {
            if (auto r = walkExprForDef(le->expression(), hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Lambda block expression: |params| { block } ───────────────
    if (auto* lbe = dynamic_cast<LucisParser::LambdaBlockExprContext*>(expr)) {
        if (lbe->paramList()) {
            for (auto* child : lbe->paramList()->children) {
                if (auto* param = dynamic_cast<LucisParser::ParamContext*>(child)) {
                    if (param->IDENTIFIER() && param->IDENTIFIER()->getSymbol() == hoveredToken)
                        return resolveIdent(tokenText, tree, bindings, cursorLine,
                                            filePath, project);
                }
            }
        }
        if (lbe->block()) {
            if (auto r = walkBlockForDef(lbe->block(), hoveredToken, tokenText, tree,
                                         bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Identifier expression (variable name, function name, etc.) ──
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        if (id->IDENTIFIER()->getSymbol() == hoveredToken) {
            return resolveIdent(tokenText, tree, bindings, cursorLine,
                                filePath, project);
        }
        return std::nullopt;
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  AST walker: statements
// ═══════════════════════════════════════════════════════════════════════

std::optional<DefinitionResult> DefinitionProvider::walkStmtForDef(
    LucisParser::StatementContext* stmt, antlr4::Token* hoveredToken,
    const std::string& tokenText, LucisParser::ProgramContext* tree,
    const CBindings& bindings, size_t cursorLine,
    const std::string& filePath, const ProjectContext* project) {

    if (!stmt || !containsToken(stmt, hoveredToken)) return std::nullopt;

    // ── Variable declaration ─────────────────────────────────────
    if (auto* vd = stmt->varDeclStmt()) {
        // Type spec
        if (auto r = resolveTypeSpecToken(vd->typeSpec(), hoveredToken, tree,
                                          bindings, filePath, project))
            return r;
        // Initializer expression (top-level and per-declarator)
        if (auto* initExpr = vd->expression()) {
            if (auto r = walkExprForDef(initExpr, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        for (auto* d : vd->varDeclarator()) {
            if (auto* initExpr = d->expression()) {
                if (auto r = walkExprForDef(initExpr, hoveredToken, tokenText, tree,
                                            bindings, cursorLine, filePath, project))
                    return r;
            }
        }
        return std::nullopt;
    }

    // ── Expression statement ─────────────────────────────────────
    if (auto* es = stmt->exprStmt()) {
        if (auto* expr = es->expression()) {
            return walkExprForDef(expr, hoveredToken, tokenText, tree,
                                  bindings, cursorLine, filePath, project);
        }
        return std::nullopt;
    }

    // ── Call statement: IDENTIFIER(args) ─────────────────────────
    if (auto* cs = stmt->callStmt()) {
        // Function name
        if (auto* id = cs->IDENTIFIER()) {
            if (id->getSymbol() == hoveredToken) {
                return resolveIdent(id->getText(), tree, bindings, cursorLine,
                                    filePath, project);
            }
        }
        // Arguments
        if (auto* args = cs->argList()) {
            for (auto* arg : args->expression()) {
                if (auto r = walkExprForDef(arg, hoveredToken, tokenText, tree,
                                            bindings, cursorLine, filePath, project))
                    return r;
            }
        }
        return std::nullopt;
    }

    // ── Return statement ─────────────────────────────────────────
    if (auto* rs = stmt->returnStmt()) {
        if (auto* expr = rs->expression()) {
            return walkExprForDef(expr, hoveredToken, tokenText, tree,
                                  bindings, cursorLine, filePath, project);
        }
        return std::nullopt;
    }

    // ── Assign statement ─────────────────────────────────────────
    if (auto* as = stmt->assignStmt()) {
        if (auto* lhs = as->expression(0)) {
            if (auto r = walkExprForDef(lhs, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        if (auto* rhs = as->expression(1)) {
            if (auto r = walkExprForDef(rhs, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Field assign statement: obj.field = expr ───────────────
    if (auto* fas = stmt->fieldAssignStmt()) {
        auto ids = fas->IDENTIFIER();
        if (!ids.empty() && ids[0] && ids[0]->getSymbol() == hoveredToken) {
            return resolveIdent(ids[0]->getText(), tree, bindings, cursorLine,
                                filePath, project);
        }

        if (ids.size() >= 2) {
            std::string currentType;
            auto* encFunc = findEnclosingFunction(tree, cursorLine);
            if (encFunc) {
                auto locals = collectLocals(encFunc, cursorLine);
                auto it = locals.find(ids[0]->getText());
                if (it != locals.end()) {
                    currentType = it->second.typeName;
                    while (!currentType.empty() && currentType.front() == '*')
                        currentType.erase(currentType.begin());
                }
            }

            for (size_t i = 1; i < ids.size(); i++) {
                auto* fid = ids[i];
                if (!fid) continue;

                if (fid->getSymbol() == hoveredToken && !currentType.empty()) {
                    if (auto r = resolveStructField(currentType, fid->getText(), tree,
                                                    bindings, filePath, project))
                        return r;
                }

                if (auto* sd = findStructDecl(tree, currentType)) {
                    std::string nextType;
                    for (auto* f : sd->structField()) {
                        if (safeText(f->IDENTIFIER()) == fid->getText()) {
                            nextType = safeText(f->typeSpec());
                            while (!nextType.empty() && nextType.front() == '*')
                                nextType.erase(nextType.begin());
                            break;
                        }
                    }
                    if (nextType.empty()) break;
                    currentType = nextType;
                } else {
                    break;
                }
            }
        }

        if (auto* rhs = fas->expression()) {
            if (auto r = walkExprForDef(rhs, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Field compound assign statement: obj.field += expr ──────
    if (auto* fcs = stmt->fieldCompoundAssignStmt()) {
        auto ids = fcs->IDENTIFIER();
        if (!ids.empty() && ids[0] && ids[0]->getSymbol() == hoveredToken) {
            return resolveIdent(ids[0]->getText(), tree, bindings, cursorLine,
                                filePath, project);
        }

        if (ids.size() >= 2) {
            std::string currentType;
            auto* encFunc = findEnclosingFunction(tree, cursorLine);
            if (encFunc) {
                auto locals = collectLocals(encFunc, cursorLine);
                auto it = locals.find(ids[0]->getText());
                if (it != locals.end()) {
                    currentType = it->second.typeName;
                    while (!currentType.empty() && currentType.front() == '*')
                        currentType.erase(currentType.begin());
                }
            }

            for (size_t i = 1; i < ids.size(); i++) {
                auto* fid = ids[i];
                if (!fid) continue;

                if (fid->getSymbol() == hoveredToken && !currentType.empty()) {
                    if (auto r = resolveStructField(currentType, fid->getText(), tree,
                                                    bindings, filePath, project))
                        return r;
                }

                if (auto* sd = findStructDecl(tree, currentType)) {
                    std::string nextType;
                    for (auto* f : sd->structField()) {
                        if (safeText(f->IDENTIFIER()) == fid->getText()) {
                            nextType = safeText(f->typeSpec());
                            while (!nextType.empty() && nextType.front() == '*')
                                nextType.erase(nextType.begin());
                            break;
                        }
                    }
                    if (nextType.empty()) break;
                    currentType = nextType;
                } else {
                    break;
                }
            }
        }

        if (auto* rhs = fcs->expression()) {
            if (auto r = walkExprForDef(rhs, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Arrow assign statement: ptr->field = expr ───────────────
    if (auto* aas = stmt->arrowAssignStmt()) {
        auto* baseId = aas->IDENTIFIER(0);
        auto* fieldId = aas->IDENTIFIER(1);

        if (baseId && baseId->getSymbol() == hoveredToken) {
            return resolveIdent(baseId->getText(), tree, bindings, cursorLine,
                                filePath, project);
        }

        if (fieldId && fieldId->getSymbol() == hoveredToken && baseId) {
            std::string structName;

            auto* encFunc = findEnclosingFunction(tree, cursorLine);
            if (encFunc) {
                auto locals = collectLocals(encFunc, cursorLine);
                auto it = locals.find(baseId->getText());
                if (it != locals.end()) {
                    structName = it->second.typeName;
                    while (!structName.empty() && structName.front() == '*')
                        structName.erase(structName.begin());
                }
            }

            // In extend instance methods, self points to the extended struct.
            if (structName.empty() && baseId->getText() == "self") {
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
                            structName = safeText(ext->IDENTIFIER());
                            break;
                        }
                    }
                    if (!structName.empty()) break;
                }
            }

            if (!structName.empty()) {
                if (auto r = resolveStructField(structName, fieldId->getText(), tree,
                                                bindings, filePath, project))
                    return r;
            }
        }

        if (auto* rhs = aas->expression()) {
            if (auto r = walkExprForDef(rhs, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Arrow compound assign statement: ptr->field += expr ─────
    if (auto* acs = stmt->arrowCompoundAssignStmt()) {
        auto* baseId = acs->IDENTIFIER(0);
        auto* fieldId = acs->IDENTIFIER(1);

        if (baseId && baseId->getSymbol() == hoveredToken) {
            return resolveIdent(baseId->getText(), tree, bindings, cursorLine,
                                filePath, project);
        }

        if (fieldId && fieldId->getSymbol() == hoveredToken && baseId) {
            std::string structName;

            auto* encFunc = findEnclosingFunction(tree, cursorLine);
            if (encFunc) {
                auto locals = collectLocals(encFunc, cursorLine);
                auto it = locals.find(baseId->getText());
                if (it != locals.end()) {
                    structName = it->second.typeName;
                    while (!structName.empty() && structName.front() == '*')
                        structName.erase(structName.begin());
                }
            }

            if (structName.empty() && baseId->getText() == "self") {
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
                            structName = safeText(ext->IDENTIFIER());
                            break;
                        }
                    }
                    if (!structName.empty()) break;
                }
            }

            if (!structName.empty()) {
                if (auto r = resolveStructField(structName, fieldId->getText(), tree,
                                                bindings, filePath, project))
                    return r;
            }
        }

        if (auto* rhs = acs->expression()) {
            if (auto r = walkExprForDef(rhs, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── If statement ─────────────────────────────────────────────
    if (auto* ifS = stmt->ifStmt()) {
        if (auto* cond = ifS->expression()) {
            if (auto r = walkExprForDef(cond, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        if (auto* body = ifS->ifBody()) {
            if (auto* b = body->block()) {
                if (auto r = walkBlockForDef(b, hoveredToken, tokenText,
                                             tree, bindings, cursorLine, filePath, project))
                    return r;
            } else if (auto* s = body->statement()) {
                if (auto r = walkStmtForDef(s, hoveredToken, tokenText,
                                            tree, bindings, cursorLine, filePath, project))
                    return r;
            }
        }
        for (auto* elif : ifS->elseIfClause()) {
            if (auto* cond = elif->expression()) {
                if (auto r = walkExprForDef(cond, hoveredToken, tokenText, tree,
                                            bindings, cursorLine, filePath, project))
                    return r;
            }
            if (auto* body = elif->ifBody()) {
                if (auto* b = body->block()) {
                    if (auto r = walkBlockForDef(b, hoveredToken, tokenText,
                                                 tree, bindings, cursorLine, filePath, project))
                        return r;
                } else if (auto* s = body->statement()) {
                    if (auto r = walkStmtForDef(s, hoveredToken, tokenText,
                                                tree, bindings, cursorLine, filePath, project))
                        return r;
                }
            }
        }
        if (auto* els = ifS->elseClause()) {
            if (auto* body = els->ifBody()) {
                if (auto* b = body->block()) {
                    if (auto r = walkBlockForDef(b, hoveredToken, tokenText,
                                                 tree, bindings, cursorLine, filePath, project))
                        return r;
                } else if (auto* s = body->statement()) {
                    if (auto r = walkStmtForDef(s, hoveredToken, tokenText,
                                                tree, bindings, cursorLine, filePath, project))
                        return r;
                }
            }
        }
        return std::nullopt;
    }

    // ── For statement ────────────────────────────────────────────
    if (auto* forS = stmt->forStmt()) {
        if (auto* fin = dynamic_cast<LucisParser::ForInStmtContext*>(forS)) {
            if (auto r = resolveTypeSpecToken(fin->typeSpec(), hoveredToken, tree,
                                              bindings, filePath, project))
                return r;
            if (auto* iterExpr = fin->expression()) {
                if (auto r = walkExprForDef(iterExpr, hoveredToken, tokenText, tree,
                                            bindings, cursorLine, filePath, project))
                    return r;
            }
            if (auto r = walkBlockForDef(fin->block(), hoveredToken, tokenText,
                                         tree, bindings, cursorLine, filePath, project))
                return r;
        }
        if (auto* fc = dynamic_cast<LucisParser::ForClassicStmtContext*>(forS)) {
            if (auto r = resolveTypeSpecToken(fc->typeSpec(), hoveredToken, tree,
                                              bindings, filePath, project))
                return r;
            for (auto* expr : fc->expression()) {
                if (auto r = walkExprForDef(expr, hoveredToken, tokenText, tree,
                                            bindings, cursorLine, filePath, project))
                    return r;
            }
            if (auto r = walkBlockForDef(fc->block(), hoveredToken, tokenText,
                                         tree, bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── While statement ──────────────────────────────────────────
    if (auto* ws = stmt->whileStmt()) {
        if (auto* cond = ws->expression()) {
            if (auto r = walkExprForDef(cond, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        if (auto r = walkBlockForDef(ws->block(), hoveredToken, tokenText,
                                     tree, bindings, cursorLine, filePath, project))
            return r;
        return std::nullopt;
    }

    // ── Do-while statement ───────────────────────────────────────
    if (auto* dw = stmt->doWhileStmt()) {
        if (auto r = walkBlockForDef(dw->block(), hoveredToken, tokenText,
                                     tree, bindings, cursorLine, filePath, project))
            return r;
        if (auto* cond = dw->expression()) {
            if (auto r = walkExprForDef(cond, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Loop statement ───────────────────────────────────────────
    if (auto* ls = stmt->loopStmt()) {
        if (auto r = walkBlockForDef(ls->block(), hoveredToken, tokenText,
                                     tree, bindings, cursorLine, filePath, project))
            return r;
        return std::nullopt;
    }

    // ── Switch statement ─────────────────────────────────────────
    if (auto* sw = stmt->switchStmt()) {
        if (auto* expr = sw->expression()) {
            if (auto r = walkExprForDef(expr, hoveredToken, tokenText, tree,
                                        bindings, cursorLine, filePath, project))
                return r;
        }
        for (auto* c : sw->caseClause()) {
            for (auto* cexpr : c->expression()) {
                if (auto r = walkExprForDef(cexpr, hoveredToken, tokenText, tree,
                                            bindings, cursorLine, filePath, project))
                    return r;
            }
            if (auto r = walkBlockForDef(c->block(), hoveredToken, tokenText,
                                         tree, bindings, cursorLine, filePath, project))
                return r;
        }
        if (auto* def = sw->defaultClause()) {
            if (auto r = walkBlockForDef(def->block(), hoveredToken, tokenText,
                                         tree, bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Try-catch-finally ────────────────────────────────────────
    if (auto* tc = stmt->tryCatchStmt()) {
        if (auto r = walkBlockForDef(tc->block(), hoveredToken, tokenText,
                                     tree, bindings, cursorLine, filePath, project))
            return r;
        for (auto* cc : tc->catchClause()) {
            // Catch type name
            if (auto r = resolveTypeSpecToken(cc->typeSpec(), hoveredToken, tree,
                                              bindings, filePath, project))
                return r;
            if (auto r = walkBlockForDef(cc->block(), hoveredToken, tokenText,
                                         tree, bindings, cursorLine, filePath, project))
                return r;
        }
        if (auto* fin = tc->finallyClause()) {
            if (auto r = walkBlockForDef(fin->block(), hoveredToken, tokenText,
                                         tree, bindings, cursorLine, filePath, project))
                return r;
        }
        return std::nullopt;
    }

    // ── Defer statement ──────────────────────────────────────────
    if (auto* ds = stmt->deferStmt()) {
        if (auto* es = ds->exprStmt()) {
            if (auto* expr = es->expression()) {
                if (auto r = walkExprForDef(expr, hoveredToken, tokenText, tree,
                                            bindings, cursorLine, filePath, project))
                    return r;
            }
        }
        return std::nullopt;
    }

    // ── Throw statement ──────────────────────────────────────────
    if (auto* ts = stmt->throwStmt()) {
        if (auto* expr = ts->expression()) {
            return walkExprForDef(expr, hoveredToken, tokenText, tree,
                                  bindings, cursorLine, filePath, project);
        }
        return std::nullopt;
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  AST walker: block
// ═══════════════════════════════════════════════════════════════════════

std::optional<DefinitionResult> DefinitionProvider::walkBlockForDef(
    LucisParser::BlockContext* block, antlr4::Token* hoveredToken,
    const std::string& tokenText, LucisParser::ProgramContext* tree,
    const CBindings& bindings, size_t cursorLine,
    const std::string& filePath, const ProjectContext* project) {

    if (!block || !containsToken(block, hoveredToken)) return std::nullopt;

    for (auto* stmt : block->statement()) {
        if (auto r = walkStmtForDef(stmt, hoveredToken, tokenText, tree,
                                    bindings, cursorLine, filePath, project))
            return r;
    }
    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════
//  collectLocals (with declToken)
// ═══════════════════════════════════════════════════════════════════════

std::unordered_map<std::string, DefinitionProvider::LocalVar>
DefinitionProvider::collectLocals(LucisParser::FunctionDeclContext* func,
                                  size_t beforeLine) {
    std::unordered_map<std::string, LocalVar> result;

    // Parameters
    if (auto* params = func->paramList()) {
        for (auto* p : params->param()) {
            if (!p->IDENTIFIER()) continue;  // skip untyped variadic (...)
            auto paramType = [](auto* pp) {
                auto t = safeText(pp->typeSpec());
                if (pp->SPREAD()) t = "[]" + t;
                return t;
            };
            std::string typeName = paramType(p);
            std::string paramName = safeText(p->IDENTIFIER());
            result[paramName] = {typeName, 0, p->IDENTIFIER()->getSymbol()};
        }
    }

    // Body locals
    collectLocalsFromBlock(func->block(), beforeLine, result);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
//  AST lookup helpers
// ═══════════════════════════════════════════════════════════════════════

LucisParser::FunctionDeclContext*
DefinitionProvider::findEnclosingFunction(LucisParser::ProgramContext* tree,
                                          size_t line) {
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

LucisParser::FunctionDeclContext*
DefinitionProvider::findFunctionDecl(LucisParser::ProgramContext* tree,
                                     const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* func = tld->functionDecl())
            if (safeIdAt(func, 0) == name)
                return func;
    }
    return nullptr;
}

LucisParser::StructDeclContext*
DefinitionProvider::findStructDecl(LucisParser::ProgramContext* tree,
                                   const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* sd = tld->structDecl())
            if (safeText(sd->IDENTIFIER()) == name)
                return sd;
    }
    return nullptr;
}

LucisParser::EnumDeclContext*
DefinitionProvider::findEnumDecl(LucisParser::ProgramContext* tree,
                                 const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* ed = tld->enumDecl())
            if (ed->IDENTIFIER() && safeText(ed->IDENTIFIER()) == name)
                return ed;
    }
    return nullptr;
}

LucisParser::UnionDeclContext*
DefinitionProvider::findUnionDecl(LucisParser::ProgramContext* tree,
                                  const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* ud = tld->unionDecl())
            if (safeText(ud->IDENTIFIER()) == name)
                return ud;
    }
    return nullptr;
}

LucisParser::TypeAliasDeclContext*
DefinitionProvider::findTypeAliasDecl(LucisParser::ProgramContext* tree,
                                      const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* ta = tld->typeAliasDecl())
            if (safeText(ta->IDENTIFIER()) == name)
                return ta;
    }
    return nullptr;
}

LucisParser::ExtendDeclContext*
DefinitionProvider::findExtendDecl(LucisParser::ProgramContext* tree,
                                   const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* ext = tld->extendDecl())
            if (safeText(ext->IDENTIFIER()) == name)
                return ext;
    }
    return nullptr;
}

LucisParser::ExternDeclContext*
DefinitionProvider::findExternDecl(LucisParser::ProgramContext* tree,
                                   const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* ext = tld->externDecl())
            if (safeText(ext->IDENTIFIER()) == name)
                return ext;
    }
    return nullptr;
}

LucisParser::ConstDeclStmtContext*
DefinitionProvider::findConstDecl(LucisParser::ProgramContext* tree,
                                  const std::string& name) {
    for (auto* tld : tree->topLevelDecl()) {
        if (auto* cd = tld->constDeclStmt()) {
            for (auto* d : cd->constDeclarator()) {
                if (d->IDENTIFIER() && safeText(d->IDENTIFIER()) == name)
                    return cd;
            }
        }
    }
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════════════

DefinitionResult DefinitionProvider::makeResult(antlr4::Token* token,
                                                const std::string& filePath) {
    DefinitionResult r;
    r.uri     = "file://" + filePath;
    r.line    = token->getLine() - 1;  // 1-based → 0-based
    r.col     = token->getCharPositionInLine();
    r.endLine = r.line;
    r.endCol  = r.col + token->getText().size();
    return r;
}

DefinitionResult DefinitionProvider::makeResult(antlr4::Token* token,
                                                const std::string& /*filePath*/,
                                                const std::string& sourceFile) {
    DefinitionResult r;
    r.uri     = "file://" + sourceFile;
    r.line    = token->getLine() - 1;
    r.col     = token->getCharPositionInLine();
    r.endLine = r.line;
    r.endCol  = r.col + token->getText().size();
    return r;
}

bool DefinitionProvider::containsToken(antlr4::ParserRuleContext* ctx,
                                       antlr4::Token* token) {
    return ::containsToken(ctx, token);
}

// ── inferExprStructType ───────────────────────────────────────────────────

std::string DefinitionProvider::inferExprStructType(
    LucisParser::ExpressionContext* expr,
    LucisParser::ProgramContext* tree,
    size_t cursorLine) {

    if (!expr) return {};

    auto stripPointers = [](std::string t) -> std::string {
        while (!t.empty() && t.front() == '*') t.erase(t.begin());
        return t;
    };

    auto lookupFuncReturnType = [&](const std::string& funcName) -> std::string {
        if (funcName.empty()) return {};

        // Same-file function
        if (auto* fd = findFunctionDecl(tree, funcName)) {
            if (fd->typeSpec()) return safeText(fd->typeSpec());
        }

        // Same-file extern declaration
        if (auto* ed = findExternDecl(tree, funcName)) {
            if (ed->typeSpec()) return safeText(ed->typeSpec());
        }

        return {};
    };

    // Simple identifier: look up in locals/params
    if (auto* ie = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        std::string varName = safeText(ie->IDENTIFIER());

        // Check function params + locals
        auto* func = findEnclosingFunction(tree, cursorLine);
        if (func) {
            auto locals = collectLocals(func, cursorLine);
            auto it = locals.find(varName);
            if (it != locals.end())
                return stripPointers(it->second.typeName);
        }

        // Check extend method params + locals
        size_t tokenLine = cursorLine + 1;
        for (auto* tld : tree->topLevelDecl()) {
            auto* ext = tld->extendDecl();
            if (!ext) continue;
            for (auto* method : ext->extendMethod()) {
                auto* ms = method->getStart();
                auto* me = method->getStop();
                if (!ms || !me) continue;
                if (tokenLine < ms->getLine() || tokenLine > me->getLine()) continue;
                // Check params
                for (auto* p : method->param()) {
                    if (safeText(p->IDENTIFIER()) == varName && p->typeSpec())
                        return stripPointers(safeText(p->typeSpec()));
                }
                // Check locals
                std::unordered_map<std::string, LocalVar> locals;
                collectLocalsFromBlock(method->block(), cursorLine, locals);
                auto it = locals.find(varName);
                if (it != locals.end())
                    return stripPointers(it->second.typeName);
            }
        }
    }

    // Function call: fn(...) -> infer from declared return type
    if (auto* fc = dynamic_cast<LucisParser::FnCallExprContext*>(expr)) {
        if (auto* calleeIdent = dynamic_cast<LucisParser::IdentExprContext*>(fc->expression())) {
            auto ret = lookupFuncReturnType(safeText(calleeIdent->IDENTIFIER()));
            if (!ret.empty()) return stripPointers(ret);
        }
    }

    // Chained field access: (expr.field) — resolve base then look up field type
    if (auto* fa = dynamic_cast<LucisParser::FieldAccessExprContext*>(expr)) {
        std::string baseType = inferExprStructType(fa->expression(), tree, cursorLine);
        if (baseType.empty()) return {};
        // Find struct, look up field type
        auto* sd = findStructDecl(tree, baseType);
        if (sd) {
            std::string fieldName = safeText(fa->IDENTIFIER());
            for (auto* f : sd->structField()) {
                if (safeText(f->IDENTIFIER()) == fieldName)
                    return stripPointers(safeText(f->typeSpec()));
            }
        }
    }

    // Chained arrow access: (expr->field)
    if (auto* aa = dynamic_cast<LucisParser::ArrowAccessExprContext*>(expr)) {
        std::string baseType = inferExprStructType(aa->expression(), tree, cursorLine);
        if (baseType.empty()) return {};
        auto* sd = findStructDecl(tree, baseType);
        if (sd) {
            std::string fieldName = safeText(aa->IDENTIFIER());
            for (auto* f : sd->structField()) {
                if (safeText(f->IDENTIFIER()) == fieldName)
                    return stripPointers(safeText(f->typeSpec()));
            }
        }
    }

    return {};
}

// ── resolveStructField ────────────────────────────────────────────────────

std::optional<DefinitionResult> DefinitionProvider::resolveStructField(
    const std::string& structName, const std::string& fieldName,
    LucisParser::ProgramContext* tree, const CBindings& bindings,
    const std::string& filePath, const ProjectContext* project) {

    // Same-file struct
    auto* sd = findStructDecl(tree, structName);
    if (sd) {
        for (auto* f : sd->structField()) {
            if (safeText(f->IDENTIFIER()) == fieldName)
                return makeResult(f->IDENTIFIER()->getSymbol(), filePath);
        }
    }

    // Cross-file struct
    if (project && project->isValid()) {
        for (auto& ns : project->registry().allModules()) {
            auto* sym = project->registry().findSymbol(ns, structName);
            if (!sym || sym->kind != ExportedSymbol::Struct) continue;
            auto* sd2 = dynamic_cast<LucisParser::StructDeclContext*>(sym->decl);
            if (!sd2) continue;
            for (auto* f : sd2->structField()) {
                if (safeText(f->IDENTIFIER()) == fieldName)
                    return makeResult(f->IDENTIFIER()->getSymbol(), filePath,
                                      sym->sourceFile);
            }
        }
    }

    (void)bindings; // reserved for C struct field resolution
    return std::nullopt;
}
