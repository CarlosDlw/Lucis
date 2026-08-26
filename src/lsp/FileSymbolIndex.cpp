#include "lsp/FileSymbolIndex.h"

#include <algorithm>

namespace {

template <typename T>
std::string idText(T* ctx, size_t i) {
    if (!ctx) return "";
    if (i >= ctx->IDENTIFIER().size()) return "";
    auto* node = ctx->IDENTIFIER(i);
    return node ? node->getText() : "";
}

}  // namespace

FileSymbolIndex::FileSymbolIndex(LucisParser::ProgramContext* tree) {
    if (!tree) return;

    for (auto* tld : tree->topLevelDecl()) {
        if (tld->getStart() && tld->getStop()) {
            DeclSpan span;
            span.startLine = static_cast<unsigned>(tld->getStart()->getLine());
            span.endLine   = static_cast<unsigned>(tld->getStop()->getLine());
            span.decl      = tld;
            declSpans_.push_back(span);
        }

        if (auto* f = tld->functionDecl()) {
            auto name = idText(f, 0);
            if (!name.empty()) functions_.emplace(name, f);
            if (f->getStart() && f->getStop()) {
                FuncSpan span;
                span.startLine = static_cast<unsigned>(f->getStart()->getLine());
                span.endLine   = static_cast<unsigned>(f->getStop()->getLine());
                span.decl      = f;
                funcSpans_.push_back(span);
            }
        } else if (auto* s = tld->structDecl()) {
            if (s->IDENTIFIER()) structs_.emplace(s->IDENTIFIER()->getText(), s);
        } else if (auto* e = tld->enumDecl()) {
            if (e->IDENTIFIER()) enums_.emplace(e->IDENTIFIER()->getText(), e);
        } else if (auto* u = tld->unionDecl()) {
            if (u->IDENTIFIER()) unions_.emplace(u->IDENTIFIER()->getText(), u);
        } else if (auto* b = tld->bitstructDecl()) {
            if (b->IDENTIFIER()) bitstructs_.emplace(b->IDENTIFIER()->getText(), b);
        } else if (auto* ta = tld->typeAliasDecl()) {
            if (ta->IDENTIFIER()) aliases_.emplace(ta->IDENTIFIER()->getText(), ta);
        } else if (auto* x = tld->extendDecl()) {
            if (x->IDENTIFIER()) extends_.emplace(x->IDENTIFIER()->getText(), x);
            for (auto* method : x->extendMethod()) {
                if (!method->getStart() || !method->getStop()) continue;
                ExtendMethodSpan span;
                span.startLine = static_cast<unsigned>(method->getStart()->getLine());
                span.endLine   = static_cast<unsigned>(method->getStop()->getLine());
                span.decl      = method;
                extendMethodSpans_.push_back(span);
            }
        } else if (auto* x = tld->externDecl()) {
            if (x->IDENTIFIER()) externs_.emplace(x->IDENTIFIER()->getText(), x);
        } else if (auto* cd = tld->constDeclStmt()) {
            for (auto* d : cd->constDeclarator()) {
                // A statement may declare several names; lookups by any of
                // them must resolve to the statement (as in the old sweep).
                if (d->IDENTIFIER() && !d->IDENTIFIER()->getText().empty())
                    consts_.emplace(d->IDENTIFIER()->getText(), cd);
            }
        }
    }

    std::sort(funcSpans_.begin(), funcSpans_.end(),
              [](const FuncSpan& a, const FuncSpan& b) {
                  return a.startLine < b.startLine;
              });
    std::sort(extendMethodSpans_.begin(), extendMethodSpans_.end(),
              [](const ExtendMethodSpan& a, const ExtendMethodSpan& b) {
                  return a.startLine < b.startLine;
              });
    std::sort(declSpans_.begin(), declSpans_.end(),
              [](const DeclSpan& a, const DeclSpan& b) {
                  return a.startLine < b.startLine;
              });
}

LucisParser::FunctionDeclContext*
FileSymbolIndex::function(const std::string& name) const {
    auto it = functions_.find(name);
    return it == functions_.end() ? nullptr : it->second;
}

LucisParser::StructDeclContext*
FileSymbolIndex::structDecl(const std::string& name) const {
    auto it = structs_.find(name);
    return it == structs_.end() ? nullptr : it->second;
}

LucisParser::EnumDeclContext*
FileSymbolIndex::enumDecl(const std::string& name) const {
    auto it = enums_.find(name);
    return it == enums_.end() ? nullptr : it->second;
}

LucisParser::UnionDeclContext*
FileSymbolIndex::unionDecl(const std::string& name) const {
    auto it = unions_.find(name);
    return it == unions_.end() ? nullptr : it->second;
}

LucisParser::BitstructDeclContext*
FileSymbolIndex::bitstructDecl(const std::string& name) const {
    auto it = bitstructs_.find(name);
    return it == bitstructs_.end() ? nullptr : it->second;
}

LucisParser::TypeAliasDeclContext*
FileSymbolIndex::typeAliasDecl(const std::string& name) const {
    auto it = aliases_.find(name);
    return it == aliases_.end() ? nullptr : it->second;
}

LucisParser::ExtendDeclContext*
FileSymbolIndex::extendDecl(const std::string& name) const {
    auto it = extends_.find(name);
    return it == extends_.end() ? nullptr : it->second;
}

LucisParser::ExternDeclContext*
FileSymbolIndex::externDecl(const std::string& name) const {
    auto it = externs_.find(name);
    return it == externs_.end() ? nullptr : it->second;
}

LucisParser::ConstDeclStmtContext*
FileSymbolIndex::constDecl(const std::string& name) const {
    auto it = consts_.find(name);
    return it == consts_.end() ? nullptr : it->second;
}

LucisParser::FunctionDeclContext*
FileSymbolIndex::enclosingFunctionAt(size_t line) const {
    if (funcSpans_.empty()) return nullptr;
    size_t tokenLine = line + 1;

    // Last span whose start line is <= tokenLine.
    auto it = std::upper_bound(
        funcSpans_.begin(), funcSpans_.end(), tokenLine,
        [](size_t line, const FuncSpan& span) { return line < span.startLine; });
    if (it == funcSpans_.begin()) return nullptr;
    --it;
    if (tokenLine <= it->endLine) return it->decl;
    return nullptr;
}

LucisParser::ExtendMethodContext*
FileSymbolIndex::enclosingExtendMethodAt(size_t line) const {
    if (extendMethodSpans_.empty()) return nullptr;
    size_t tokenLine = line + 1;

    auto it = std::upper_bound(
        extendMethodSpans_.begin(), extendMethodSpans_.end(), tokenLine,
        [](size_t line, const ExtendMethodSpan& span) {
            return line < span.startLine;
        });
    if (it == extendMethodSpans_.begin()) return nullptr;
    --it;
    if (tokenLine <= it->endLine) return it->decl;
    return nullptr;
}

LucisParser::TopLevelDeclContext*
FileSymbolIndex::enclosingDeclAt(size_t line) const {
    if (declSpans_.empty()) return nullptr;
    size_t tokenLine = line + 1;

    auto it = std::upper_bound(
        declSpans_.begin(), declSpans_.end(), tokenLine,
        [](size_t line, const DeclSpan& span) { return line < span.startLine; });
    if (it == declSpans_.begin()) return nullptr;
    --it;
    if (tokenLine <= it->endLine) return it->decl;
    return nullptr;
}