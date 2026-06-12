#include "helpc/HelpCParser.h"

#include <clang-c/Index.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

// ── Short name → header mapping ─────────────────────────────────────────────

static const std::unordered_map<std::string, std::string>& knownShortNames() {
    static const std::unordered_map<std::string, std::string> table = {
        {"raylib",      "raylib.h"},
        {"SDL2",        "SDL2/SDL.h"},
        {"SDL",         "SDL2/SDL.h"},
        {"sdl2",        "SDL2/SDL.h"},
        {"glfw",        "GLFW/glfw3.h"},
        {"opengl",      "GL/gl.h"},
        {"gl",          "GL/gl.h"},
        {"vulkan",      "vulkan/vulkan.h"},
        {"curl",        "curl/curl.h"},
        {"openssl",     "openssl/ssl.h"},
        {"zlib",        "zlib.h"},
        {"png",         "png.h"},
        {"jpeg",        "jpeglib.h"},
        {"sqlite",      "sqlite3.h"},
        {"sqlite3",     "sqlite3.h"},
        {"postgres",    "libpq-fe.h"},
        {"pq",          "libpq-fe.h"},
        {"mysql",       "mysql/mysql.h"},
        {"readline",    "readline/readline.h"},
        {"ncurses",     "ncurses.h"},
        {"xml2",        "libxml/parser.h"},
        {"yaml",        "yaml.h"},
        {"portaudio",   "portaudio.h"},
        {"sndfile",     "sndfile.h"},
        {"zstd",        "zstd.h"},
        {"math",        "math.h"},
        {"stdio",       "stdio.h"},
        {"stdlib",      "stdlib.h"},
        {"string",      "string.h"},
        {"strings",     "strings.h"},
        {"pthread",     "pthread.h"},
        {"unistd",      "unistd.h"},
        {"fcntl",       "fcntl.h"},
        {"signal",      "signal.h"},
        {"errno",       "errno.h"},
        {"time",        "time.h"},
        {"dirent",      "dirent.h"},
        {"sys/socket",  "sys/socket.h"},
        {"sys/stat",    "sys/stat.h"},
    };
    return table;
}

// ── Header → link flag mapping ──────────────────────────────────────────────

static const std::unordered_map<std::string, std::string>& knownLinkFlags() {
    static const std::unordered_map<std::string, std::string> table = {
        {"raylib.h",            "-lraylib"},
        {"SDL2/SDL.h",          "-lSDL2"},
        {"SDL.h",               "-lSDL2"},
        {"GLFW/glfw3.h",        "-lglfw"},
        {"GL/gl.h",             "-lGL"},
        {"vulkan/vulkan.h",     "-lvulkan"},
        {"curl/curl.h",         "-lcurl"},
        {"openssl/ssl.h",       "-lssl"},
        {"openssl/crypto.h",    "-lcrypto"},
        {"zlib.h",              "-lz"},
        {"zstd.h",              "-lzstd"},
        {"png.h",               "-lpng"},
        {"jpeglib.h",           "-ljpeg"},
        {"sqlite3.h",           "-lsqlite3"},
        {"libpq-fe.h",          "-lpq"},
        {"mysql/mysql.h",       "-lmysqlclient"},
        {"readline/readline.h", "-lreadline"},
        {"ncurses.h",           "-lncurses"},
        {"libxml/parser.h",     "-lxml2"},
        {"yaml.h",              "-lyaml"},
        {"portaudio.h",         "-lportaudio"},
        {"sndfile.h",           "-lsndfile"},
        {"math.h",              "-lm"},
        {"pthread.h",           "-lpthread"},
    };
    return table;
}

// ── System include discovery ────────────────────────────────────────────────

void HelpCParser::discoverSystemIncludes() {
    if (!systemIncludes_.empty()) return;

    std::array<char, 4096> buffer;
    std::string output;

    FILE* pipe = popen("clang -E -x c -v /dev/null 2>&1", "r");
    if (!pipe) return;

    while (fgets(buffer.data(), buffer.size(), pipe))
        output += buffer.data();
    pclose(pipe);

    auto startMarker = output.find("#include <...> search starts here:");
    auto endMarker   = output.find("End of search list.");
    if (startMarker == std::string::npos || endMarker == std::string::npos)
        return;

    std::istringstream stream(output.substr(
        startMarker + 34, endMarker - startMarker - 34));
    std::string line;
    while (std::getline(stream, line)) {
        auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        auto trimmed = line.substr(first);
        if (!trimmed.empty())
            systemIncludes_.push_back("-isystem" + trimmed);
    }
}

// ── Lucis type name from C type spelling ────────────────────────────────────────

static std::string cTypeToLucisType(CXType cxType) {
    CXTypeKind kind = cxType.kind;

    switch (kind) {
    case CXType_Void:     return "void";
    case CXType_Bool:     return "bool";
    case CXType_Char_U:
    case CXType_UChar:    return "uint8";
    case CXType_Char_S:
    case CXType_SChar:    return "int8";
    case CXType_UShort:   return "uint16";
    case CXType_Short:    return "int16";
    case CXType_UInt:     return "uint32";
    case CXType_Int:      return "int32";
    case CXType_ULong:
    case CXType_ULongLong: return "uint64";
    case CXType_Long:
    case CXType_LongLong: return "int64";
    case CXType_Float:    return "float32";
    case CXType_Double:   return "float64";
    case CXType_LongDouble: return "float64";
    case CXType_Pointer: {
        CXType pointee = clang_getPointeeType(cxType);
        if (pointee.kind == CXType_Char_S || pointee.kind == CXType_Char_U ||
            pointee.kind == CXType_SChar || pointee.kind == CXType_UChar) {
            // Check for const
            if (clang_isConstQualifiedType(pointee))
                return "*char";
            return "*char";
        }
        if (pointee.kind == CXType_Void) return "*void";
        return "*" + cTypeToLucisType(pointee);
    }
    case CXType_Elaborated: {
        CXType named = clang_Type_getNamedType(cxType);
        return cTypeToLucisType(named);
    }
    case CXType_Record: {
        CXString spelling = clang_getTypeSpelling(cxType);
        std::string s = clang_getCString(spelling);
        clang_disposeString(spelling);
        // Strip "struct " or "union " prefix
        if (s.rfind("struct ", 0) == 0) s = s.substr(7);
        if (s.rfind("union ", 0) == 0) s = s.substr(6);
        return s;
    }
    case CXType_Enum: {
        CXString spelling = clang_getTypeSpelling(cxType);
        std::string s = clang_getCString(spelling);
        clang_disposeString(spelling);
        if (s.rfind("enum ", 0) == 0) s = s.substr(5);
        return s;
    }
    case CXType_Typedef: {
        CXString spelling = clang_getTypeSpelling(cxType);
        std::string s = clang_getCString(spelling);
        clang_disposeString(spelling);
        return s;
    }
    case CXType_ConstantArray:
    case CXType_IncompleteArray: {
        CXType elemType = clang_getArrayElementType(cxType);
        return "*" + cTypeToLucisType(elemType);
    }
    case CXType_FunctionProto:
    case CXType_FunctionNoProto:
        return "fn(...)";
    default: {
        CXString spelling = clang_getTypeSpelling(cxType);
        std::string s = clang_getCString(spelling);
        clang_disposeString(spelling);
        return s;
    }
    }
}

static std::string getCTypeSpelling(CXType cxType) {
    CXString spelling = clang_getTypeSpelling(cxType);
    std::string s = clang_getCString(spelling);
    clang_disposeString(spelling);
    return s;
}

static std::string getCursorDoc(CXCursor cursor) {
    CXString brief = clang_Cursor_getBriefCommentText(cursor);
    const char* text = clang_getCString(brief);
    std::string result;
    if (text && text[0] != '\0')
        result = text;
    clang_disposeString(brief);
    return result;
}

// ── Macro token evaluation (reused from CHeaderResolver) ────────────────────

static bool tryEvalMacroTokens(const std::vector<std::string>& tokens,
                               int64_t& outValue, bool& outIsNull) {
    outValue = 0;
    outIsNull = false;

    if (tokens.empty()) return false;

    // NULL pattern: ((void*)0) or ((void *)0) or (void*)0
    {
        std::string joined;
        for (auto& t : tokens) joined += t;
        // Remove all spaces
        std::string compact;
        for (char c : joined) if (c != ' ') compact += c;
        if (compact == "((void*)0)" || compact == "(void*)0" ||
            compact == "((void*)0L)" || compact == "(void*)0L" ||
            compact == "__null" || compact == "nullptr") {
            outIsNull = true;
            outValue  = 0;
            return true;
        }
    }

    // Single token: integer literal or character literal
    if (tokens.size() == 1) {
        const std::string& tok = tokens[0];
        if (tok.empty()) return false;

        // Character literal
        if (tok.size() >= 3 && tok.front() == '\'' && tok.back() == '\'') {
            if (tok.size() == 3) {
                outValue = static_cast<unsigned char>(tok[1]);
                return true;
            }
            if (tok.size() == 4 && tok[1] == '\\') {
                switch (tok[2]) {
                case 'n': outValue = '\n'; return true;
                case 't': outValue = '\t'; return true;
                case 'r': outValue = '\r'; return true;
                case '0': outValue = '\0'; return true;
                case '\\': outValue = '\\'; return true;
                case '\'': outValue = '\''; return true;
                default: return false;
                }
            }
            return false;
        }

        // Integer literal
        try {
            size_t pos = 0;
            unsigned long long v = std::stoull(tok, &pos, 0);
            // Skip trailing suffixes: U, L, UL, ULL, LL
            outValue = static_cast<int64_t>(v);
            return true;
        } catch (...) {
            return false;
        }
    }

    // Simple binary expression: A op B
    if (tokens.size() == 3) {
        int64_t a, b;
        bool dummy;
        std::vector<std::string> ta = {tokens[0]};
        std::vector<std::string> tb = {tokens[2]};
        if (!tryEvalMacroTokens(ta, a, dummy)) return false;
        if (!tryEvalMacroTokens(tb, b, dummy)) return false;

        const std::string& op = tokens[1];
        if (op == "+") { outValue = a + b; return true; }
        if (op == "-") { outValue = a - b; return true; }
        if (op == "*") { outValue = a * b; return true; }
        if (op == "/") { outValue = b != 0 ? a / b : 0; return true; }
        if (op == "|") { outValue = a | b; return true; }
        if (op == "&") { outValue = a & b; return true; }
        if (op == "<<") { outValue = a << b; return true; }
        if (op == ">>") { outValue = a >> b; return true; }
    }

    // Parenthesized expression: ( expr )
    if (tokens.size() >= 3 && tokens.front() == "(" && tokens.back() == ")") {
        std::vector<std::string> inner(tokens.begin() + 1, tokens.end() - 1);
        return tryEvalMacroTokens(inner, outValue, outIsNull);
    }

    // Unary complement: ~ X
    if (tokens.size() == 2 && tokens[0] == "~") {
        int64_t v;
        bool dummy;
        std::vector<std::string> tv = {tokens[1]};
        if (tryEvalMacroTokens(tv, v, dummy)) {
            outValue = ~v;
            return true;
        }
    }

    return false;
}

static bool tryEvalStructLiteralMacro(const std::vector<std::string>& tokens,
                                      std::string& outType,
                                      std::vector<int64_t>& outValues) {
    outValues.clear();
    size_t i = 0;

    // Pattern 1: CLITERAL(Type){ val, val, ... }
    // Pattern 2: (Type){ val, val, ... }
    if (i < tokens.size() && (tokens[i] == "CLITERAL" || tokens[i] == "CLIT")) {
        i++; // skip CLITERAL
    }

    if (i >= tokens.size() || tokens[i] != "(") return false;
    i++; // skip (
    if (i >= tokens.size()) return false;
    outType = tokens[i];
    i++; // skip Type
    if (i >= tokens.size() || tokens[i] != ")") return false;
    i++; // skip )
    if (i >= tokens.size() || tokens[i] != "{") return false;
    i++; // skip {

    while (i < tokens.size() && tokens[i] != "}") {
        if (tokens[i] == ",") { i++; continue; }
        try {
            size_t pos = 0;
            long long v = std::stoll(tokens[i], &pos, 0);
            outValues.push_back(static_cast<int64_t>(v));
        } catch (...) {
            // Try negative: if previous was '-' ...
            return false;
        }
        i++;
    }

    return !outType.empty() && !outValues.empty();
}

// ── Visitor data ────────────────────────────────────────────────────────────

namespace {
struct VisitorData {
    HelpCHeaderInfo* info;
    std::unordered_set<std::string> seen;
};

static CXChildVisitResult visitor(CXCursor cursor, CXCursor /*parent*/,
                                  CXClientData clientData) {
    auto* data = static_cast<VisitorData*>(clientData);
    CXCursorKind kind = clang_getCursorKind(cursor);

    // Skip internal/compiler symbols
    CXString nameStr = clang_getCursorSpelling(cursor);
    std::string name = clang_getCString(nameStr);
    clang_disposeString(nameStr);

    if (name.empty() || name[0] == '_')
        return CXChildVisit_Continue;

    // ── Function declarations ───────────────────────────────────────────
    if (kind == CXCursor_FunctionDecl) {
        if (data->seen.count("fn:" + name)) return CXChildVisit_Continue;
        data->seen.insert("fn:" + name);

        CXType funcType = clang_getCursorType(cursor);
        CXType retType  = clang_getResultType(funcType);

        HelpCFunction fn;
        fn.name        = name;
        fn.returnCType = getCTypeSpelling(retType);
        fn.returnLucisType = cTypeToLucisType(retType);
        fn.isVariadic  = clang_isFunctionTypeVariadic(funcType);
        fn.doc         = getCursorDoc(cursor);

        int numArgs = clang_Cursor_getNumArguments(cursor);
        for (int i = 0; i < numArgs; i++) {
            CXCursor arg = clang_Cursor_getArgument(cursor, i);
            CXType argType = clang_getCursorType(arg);

            CXString argNameStr = clang_getCursorSpelling(arg);
            std::string argName = clang_getCString(argNameStr);
            clang_disposeString(argNameStr);

            if (argName.empty())
                argName = "arg" + std::to_string(i);

            HelpCParam param;
            param.name  = argName;
            param.cType = getCTypeSpelling(argType);
            param.lucisType = cTypeToLucisType(argType);
            fn.params.push_back(std::move(param));
        }

        data->info->functions.push_back(std::move(fn));
        return CXChildVisit_Continue;
    }

    // ── Struct/Union declarations ───────────────────────────────────────
    if (kind == CXCursor_StructDecl || kind == CXCursor_UnionDecl) {
        if (name.empty()) return CXChildVisit_Continue;
        if (data->seen.count("struct:" + name)) return CXChildVisit_Continue;

        // Skip forward declarations
        if (!clang_isCursorDefinition(cursor))
            return CXChildVisit_Continue;

        data->seen.insert("struct:" + name);

        CXType structCXType = clang_getCursorType(cursor);

        HelpCStruct s;
        s.name      = name;
        s.isUnion   = (kind == CXCursor_UnionDecl);
        s.size      = clang_Type_getSizeOf(structCXType);
        s.alignment = clang_Type_getAlignOf(structCXType);
        s.doc       = getCursorDoc(cursor);

        // Visit fields
        struct FieldVisitorData {
            HelpCStruct* s;
            CXType parentType;
        } fvd{&s, structCXType};

        clang_visitChildren(cursor,
            [](CXCursor child, CXCursor /*parent*/, CXClientData cd)
                -> CXChildVisitResult {
                auto* fd = static_cast<FieldVisitorData*>(cd);
                if (clang_getCursorKind(child) != CXCursor_FieldDecl)
                    return CXChildVisit_Continue;

                CXString fieldNameStr = clang_getCursorSpelling(child);
                std::string fieldName = clang_getCString(fieldNameStr);
                clang_disposeString(fieldNameStr);

                CXType fieldCXType = clang_getCursorType(child);

                HelpCField field;
                field.name   = fieldName;
                field.cType  = getCTypeSpelling(fieldCXType);
                field.lucisType  = cTypeToLucisType(fieldCXType);
                field.size   = clang_Type_getSizeOf(fieldCXType);

                // Get field offset in bytes
                long long offsetBits = clang_Type_getOffsetOf(
                    fd->parentType, fieldName.c_str());
                if (offsetBits >= 0)
                    field.offset = offsetBits / 8;

                fd->s->fields.push_back(std::move(field));
                return CXChildVisit_Continue;
            }, &fvd);

        data->info->structs.push_back(std::move(s));
        return CXChildVisit_Continue;
    }

    // ── Enum declarations ───────────────────────────────────────────────
    if (kind == CXCursor_EnumDecl) {
        if (name.empty()) return CXChildVisit_Continue;
        if (data->seen.count("enum:" + name)) return CXChildVisit_Continue;
        data->seen.insert("enum:" + name);

        HelpCEnum e;
        e.name = name;
        e.doc  = getCursorDoc(cursor);

        clang_visitChildren(cursor,
            [](CXCursor child, CXCursor /*parent*/, CXClientData cd)
                -> CXChildVisitResult {
                auto* ep = static_cast<HelpCEnum*>(cd);
                if (clang_getCursorKind(child) != CXCursor_EnumConstantDecl)
                    return CXChildVisit_Continue;

                CXString valNameStr = clang_getCursorSpelling(child);
                std::string valName = clang_getCString(valNameStr);
                clang_disposeString(valNameStr);

                int64_t val = clang_getEnumConstantDeclValue(child);
                ep->values.push_back({valName, val});
                return CXChildVisit_Continue;
            }, &e);

        data->info->enums.push_back(std::move(e));
        return CXChildVisit_Continue;
    }

    // ── Typedef declarations ────────────────────────────────────────────
    if (kind == CXCursor_TypedefDecl) {
        if (data->seen.count("typedef:" + name)) return CXChildVisit_Continue;
        data->seen.insert("typedef:" + name);

        CXType underType = clang_getTypedefDeclUnderlyingType(cursor);

        HelpCTypedef td;
        td.name            = name;
        td.underlyingCType = getCTypeSpelling(underType);
        td.underlyingLucisType = cTypeToLucisType(underType);
        td.doc             = getCursorDoc(cursor);
        data->info->typedefs.push_back(std::move(td));
        return CXChildVisit_Continue;
    }

    // ── Macro definitions ───────────────────────────────────────────────
    if (kind == CXCursor_MacroDefinition) {
        if (data->seen.count("macro:" + name)) return CXChildVisit_Continue;
        data->seen.insert("macro:" + name);

        CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
        CXSourceRange range = clang_getCursorExtent(cursor);
        CXToken* tokens = nullptr;
        unsigned numTokens = 0;
        clang_tokenize(tu, range, &tokens, &numTokens);

        std::vector<std::string> bodyTokens;
        for (unsigned ti = 1; ti < numTokens; ti++) {
            CXString ts = clang_getTokenSpelling(tu, tokens[ti]);
            bodyTokens.push_back(clang_getCString(ts));
            clang_disposeString(ts);
        }
        clang_disposeTokens(tu, tokens, numTokens);

        // Skip function-like macros (no body or first token is '(')
        if (bodyTokens.empty())
            return CXChildVisit_Continue;

        int64_t value = 0;
        bool isNull = false;
        if (tryEvalMacroTokens(bodyTokens, value, isNull)) {
            HelpCMacro m;
            m.name   = name;
            m.value  = value;
            m.isNull = isNull;
            data->info->macros.push_back(std::move(m));
        } else {
            std::string structType;
            std::vector<int64_t> fieldValues;
            if (tryEvalStructLiteralMacro(bodyTokens, structType, fieldValues)) {
                HelpCStructMacro sm;
                sm.name        = name;
                sm.structType  = structType;
                sm.fieldValues = std::move(fieldValues);
                data->info->structMacros.push_back(std::move(sm));
            }
        }
        return CXChildVisit_Continue;
    }

    // ── Global variables ────────────────────────────────────────────────
    if (kind == CXCursor_VarDecl) {
        CXLinkageKind linkage = clang_getCursorLinkage(cursor);
        if (linkage != CXLinkage_External)
            return CXChildVisit_Continue;
        if (data->seen.count("global:" + name)) return CXChildVisit_Continue;
        data->seen.insert("global:" + name);

        CXType varType = clang_getCursorType(cursor);

        HelpCGlobal g;
        g.name  = name;
        g.cType = getCTypeSpelling(varType);
        g.lucisType = cTypeToLucisType(varType);
        g.doc   = getCursorDoc(cursor);
        data->info->globals.push_back(std::move(g));
        return CXChildVisit_Continue;
    }

    return CXChildVisit_Continue;
}

} // anonymous namespace

// ── Constructor ─────────────────────────────────────────────────────────────

HelpCParser::HelpCParser(const std::vector<std::string>& includePaths)
    : includePaths_(includePaths) {}

// ── Short name resolution ───────────────────────────────────────────────────

std::string HelpCParser::resolveShortName(const std::string& input) {
    // If it already has a .h extension, use as-is
    if (input.size() > 2 && input.substr(input.size() - 2) == ".h")
        return input;

    // Check known short names
    auto it = knownShortNames().find(input);
    if (it != knownShortNames().end())
        return it->second;

    // Try appending .h
    return input + ".h";
}

// ── Main parse method ───────────────────────────────────────────────────────

bool HelpCParser::parse(const std::string& headerName, HelpCHeaderInfo& out) {
    discoverSystemIncludes();

    std::string resolved = resolveShortName(headerName);
    out.headerName = resolved;

    // Look up link flag
    auto lit = knownLinkFlags().find(resolved);
    if (lit != knownLinkFlags().end())
        out.linkFlag = lit->second;

    // Build the source file content
    std::string source = "#include <" + resolved + ">\n";

    // Build clang arguments
    std::vector<const char*> clangArgs;
    clangArgs.push_back("-x");
    clangArgs.push_back("c");
    clangArgs.push_back("-std=c11");
    // Enable doc comment parsing
    clangArgs.push_back("-fparse-all-comments");

    for (auto& inc : systemIncludes_)
        clangArgs.push_back(inc.c_str());
    for (auto& inc : includePaths_) {
        // Store -I flags persistently
        clangArgs.push_back("-I");
        clangArgs.push_back(inc.c_str());
    }

    // Create translation unit from unsaved file
    CXIndex index = clang_createIndex(0, 0);
    CXUnsavedFile unsaved;
    unsaved.Filename = "helpc_input.c";
    unsaved.Contents = source.c_str();
    unsaved.Length   = source.size();

    CXTranslationUnit tu = clang_parseTranslationUnit(
        index,
        "helpc_input.c",
        clangArgs.data(),
        static_cast<int>(clangArgs.size()),
        &unsaved, 1,
        CXTranslationUnit_DetailedPreprocessingRecord |
        CXTranslationUnit_SkipFunctionBodies);

    if (!tu) {
        std::cerr << "lucis helpc: failed to parse header '" << resolved << "'\n";
        clang_disposeIndex(index);
        return false;
    }

    // Check for errors
    unsigned numDiag = clang_getNumDiagnostics(tu);
    bool hasError = false;
    for (unsigned i = 0; i < numDiag; i++) {
        CXDiagnostic diag = clang_getDiagnostic(tu, i);
        CXDiagnosticSeverity sev = clang_getDiagnosticSeverity(diag);
        if (sev >= CXDiagnostic_Error) {
            hasError = true;
            CXString msg = clang_getDiagnosticSpelling(diag);
            std::cerr << "lucis helpc: " << clang_getCString(msg) << "\n";
            clang_disposeString(msg);
        }
        clang_disposeDiagnostic(diag);
    }

    if (hasError) {
        clang_disposeTranslationUnit(tu);
        clang_disposeIndex(index);
        return false;
    }

    // Visit the AST
    VisitorData vdata{&out, {}};
    CXCursor rootCursor = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(rootCursor, visitor, &vdata);

    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);
    return true;
}
