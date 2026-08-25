#include "attributes/TestAttribute.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace {

// Accepts "..." (STR_LIT) and c"..." (C_STR_LIT); common escapes are decoded.
std::string unquoteStringLiteral(const std::string& raw) {
    size_t i = 0;
    if (raw.size() >= 2 && raw[0] == 'c' && raw[1] == '"') {
        i = 2;
    } else if (!raw.empty() && raw[0] == '"') {
        i = 1;
    } else {
        return raw;
    }

    std::string out;
    out.reserve(raw.size());
    for (; i < raw.size(); ++i) {
        char c = raw[i];
        if (c == '"') break;  // closing quote
        if (c == '\\' && i + 1 < raw.size()) {
            char e = raw[++i];
            switch (e) {
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case '0':  out += '\0'; break;
                case '\\': out += '\\'; break;
                case '"':  out += '"';  break;
                case '\'': out += '\''; break;
                default:   out += '\\'; out += e; break;  // keep unknown escapes verbatim
            }
        } else {
            out += c;
        }
    }
    return out;
}

// Read an integer attrArg (dec/hex/oct/bin, optional leading '-'; digit
// separators '_' are ignored). Returns false when the argument is not an int.
bool readIntValue(LucisParser::AttrArgContext* arg, int64_t& out) {
    if (!arg) return false;
    bool neg = arg->MINUS() != nullptr;
    antlr4::tree::TerminalNode* tok = arg->MINUS()
        ? arg->INT_LIT()
        : (arg->INT_LIT() ? arg->INT_LIT()
           : (arg->HEX_LIT()   ? arg->HEX_LIT()
              : (arg->OCT_LIT() ? arg->OCT_LIT()
                 : arg->BIN_LIT())));
    if (!tok) return false;

    std::string text = tok->getText();
    text.erase(std::remove(text.begin(), text.end(), '_'), text.end());

    int base = 10;
    if (text.size() > 2 && text[0] == '0') {
        switch (text[1]) {
            case 'x': case 'X': base = 16; text = text.substr(2); break;
            case 'o': case 'O': base = 8;  text = text.substr(2); break;
            case 'b': case 'B': base = 2;  text = text.substr(2); break;
            default: break;
        }
    }

    char* end = nullptr;
    long long v = std::strtoll(text.c_str(), &end, base);
    if (!end || *end != '\0') return false;
    out = neg ? -v : v;
    return true;
}

}  // namespace

bool populateAttribute(LucisParser::AttributeContext* ctx, Attribute& out) {
    if (!ctx || !ctx->IDENTIFIER()) return false;
    out.name = ctx->IDENTIFIER()->getText();
    out.line = ctx->getStart() ? ctx->getStart()->getLine() : 0;
    out.col  = ctx->getStart() ? ctx->getStart()->getCharPositionInLine() : 0;
    out.rawCtx = ctx;
    if (auto* argList = ctx->attrArgList()) {
        for (auto* arg : argList->attrArg()) {
            // Key-value and call forms are handled via rawCtx by complex
            // attributes (e.g. #[cfg(...)]); flat args cover simple literals.
            if (arg->ASSIGN() || arg->LPAREN()) continue;
            if (arg->IDENTIFIER()) {
                out.args.push_back({AttributeArg::Ident, arg->getText(), "", 0, 0.0});
            } else if (arg->STR_LIT() || arg->C_STR_LIT()) {
                out.args.push_back({AttributeArg::String, "", arg->getText(), 0, 0.0});
            } else if (arg->INT_LIT() || arg->HEX_LIT() || arg->OCT_LIT() || arg->BIN_LIT()) {
                int64_t v = 0;
                readIntValue(arg, v);
                out.args.push_back({AttributeArg::Int, "", "", v, 0.0});
            } else if (arg->FLOAT_LIT()) {
                out.args.push_back({AttributeArg::Float, "", "", 0, std::stod(arg->getText())});
            } else if (arg->BOOL_LIT()) {
                out.args.push_back({AttributeArg::Int, "", "", arg->getText() == "true" ? 1 : 0, 0.0});
            }
        }
    }
    return true;
}

LucisParser::AttributeContext* findAttribute(
    LucisParser::AttributeListContext* attrs, const std::string& name) {
    if (!attrs) return nullptr;
    for (auto* a : attrs->attribute()) {
        if (a->IDENTIFIER() && a->IDENTIFIER()->getText() == name)
            return a;
    }
    return nullptr;
}

bool parseTestSpec(const Attribute& attr, TestSpec& out, std::vector<std::string>& errors) {
    if (!attr.rawCtx) return true;  // bare #[test] / #[test()]
    auto* listCtx = attr.rawCtx->getRuleContext<LucisParser::AttrArgListContext>(0);
    if (!listCtx) return true;

    auto dup = [&](const char* what) {
        errors.push_back(std::string("duplicate #[test] ") + what);
    };

    bool seenName = false, seenSkip = false, seenExpectedFail = false, seenTimeout = false;
    for (auto* arg : listCtx->attrArg()) {
        // Display name: a bare string literal.
        if (arg->STR_LIT() || arg->C_STR_LIT()) {
            if (seenName) { dup("name"); return false; }
            out.displayName = unquoteStringLiteral(arg->getText());
            seenName = true;
            continue;
        }

        // Everything else starts with an identifier key.
        if (!arg->IDENTIFIER()) {
            errors.push_back("unexpected argument '" + arg->getText() + "' in #[test]");
            return false;
        }
        std::string key = arg->IDENTIFIER()->getText();

        // key = value
        if (arg->ASSIGN()) {
            auto* value = arg->attrArg();
            if (key == "timeout") {
                if (seenTimeout) { dup("timeout"); return false; }
                int64_t ms = 0;
                if (!readIntValue(value, ms) || ms <= 0) {
                    errors.push_back("timeout must be a positive integer (milliseconds)");
                    return false;
                }
                out.timeoutMs = ms;
                seenTimeout = true;
            } else {
                errors.push_back("unknown #[test] option '" + key + "'");
                return false;
            }
            continue;
        }

        // key(args)
        if (arg->LPAREN()) {
            auto* inner = arg->attrArgList();
            if (key == "skip") {
                if (seenSkip) { dup("skip"); return false; }
                if (inner && !inner->attrArg().empty()) {
                    if (inner->attrArg().size() != 1 || !inner->attrArg(0)->STR_LIT()) {
                        errors.push_back("skip() takes an optional single string reason");
                        return false;
                    }
                    out.skipReason = unquoteStringLiteral(inner->attrArg(0)->getText());
                }
                out.skip = true;
                seenSkip = true;
            } else if (key == "timeout") {
                if (seenTimeout) { dup("timeout"); return false; }
                if (!inner || inner->attrArg().size() != 1) {
                    errors.push_back("timeout() takes exactly one integer (milliseconds)");
                    return false;
                }
                int64_t ms = 0;
                if (!readIntValue(inner->attrArg(0), ms) || ms <= 0) {
                    errors.push_back("timeout must be a positive integer (milliseconds)");
                    return false;
                }
                out.timeoutMs = ms;
                seenTimeout = true;
            } else {
                errors.push_back("unknown #[test] option '" + key + "'");
                return false;
            }
            continue;
        }

        // Bare flag.
        if (key == "skip") {
            if (seenSkip) { dup("skip"); return false; }
            out.skip = true;
            seenSkip = true;
        } else if (key == "expected_fail") {
            if (seenExpectedFail) { dup("expected_fail"); return false; }
            out.expectedFail = true;
            seenExpectedFail = true;
        } else {
            errors.push_back("unknown #[test] option '" + key +
                             "' (valid: \"name\", skip, skip(\"reason\"), "
                             "expected_fail, timeout(N))");
            return false;
        }
    }
    return true;
}