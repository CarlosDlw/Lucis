#include "types/BuiltinRegistry.h"

void BuiltinRegistry::add(std::string name, std::string returnType,
                           std::vector<std::string> params, bool poly,
                           bool variadic, bool returnsOwned,
                           std::vector<size_t> consumingArgs,
                           std::vector<size_t> borrowedArgs,
                           std::string description) {
    BuiltinSignature sig;
    sig.name = name;
    sig.returnType = returnType;
    sig.paramTypes = std::move(params);
    sig.isPolymorphic = poly;
    sig.isVariadic = variadic;
    sig.returnsOwned = returnsOwned;
    sig.consumingArgs = std::move(consumingArgs);
    sig.borrowedArgs = std::move(borrowedArgs);
    sig.description = std::move(description);
    signatures_[std::move(name)] = std::move(sig);
}

const BuiltinSignature* BuiltinRegistry::lookup(const std::string& name) const {
    auto it = signatures_.find(name);
    if (it != signatures_.end()) return &it->second;
    return nullptr;
}

void BuiltinRegistry::addConstant(std::string name, std::string typeName) {
    constants_[std::move(name)] = std::move(typeName);
}

const std::string& BuiltinRegistry::lookupConstant(const std::string& name) const {
    static const std::string empty;
    auto it = constants_.find(name);
    if (it != constants_.end()) return it->second;
    return empty;
}

bool BuiltinRegistry::returnsOwned(const std::string& name) const {
    auto it = signatures_.find(name);
    return it != signatures_.end() ? it->second.returnsOwned : false;
}

bool BuiltinRegistry::argConsumes(const std::string& name, size_t argIndex) const {
    auto it = signatures_.find(name);
    if (it == signatures_.end()) return false;
    for (size_t i : it->second.consumingArgs) {
        if (i == argIndex) return true;
    }
    return false;
}

bool BuiltinRegistry::argBorrows(const std::string& name, size_t argIndex) const {
    auto it = signatures_.find(name);
    if (it == signatures_.end()) return false;
    for (size_t i : it->second.borrowedArgs) {
        if (i == argIndex) return true;
    }
    return false;
}

BuiltinRegistry::BuiltinRegistry() {
    // ═════════════════════════════════════════════════════════════════════
    // std::log — polymorphic print functions (1 arg, any type)
    // ═════════════════════════════════════════════════════════════════════
    add("println",   "void", {"_any"}, true);
    add("print",     "void", {"_any"}, true);
    add("eprintln",  "void", {"_any"}, true);
    add("eprint",    "void", {"_any"}, true);
    add("dbg",       "_any", {"_any"}, true);
    add("sprintf",   "string", {"string"}, true, true, true);

    // ═════════════════════════════════════════════════════════════════════
    // std::io — input/output
    // ═════════════════════════════════════════════════════════════════════
    add("readLine",       "string", {}, false, false, true);
    add("readChar",       "char",   {});
    add("readInt",        "int64",  {});
    add("readFloat",      "float64", {});
    add("readBool",       "bool",   {});
    add("readAll",        "string", {}, false, false, true);
    add("prompt",         "string", {"string"}, false, false, true, {}, {0});
    add("promptInt",      "int64",  {"string"});
    add("promptFloat",    "float64", {"string"});
    add("promptBool",     "bool",   {"string"});
    add("readByte",       "uint8",  {});
    add("readLines",      "Vec<string>", {});
    add("readNBytes",     "Vec<uint8>",  {"usize"});
    add("isEOF",          "bool",   {});
    add("readPassword",   "string", {}, false, false, true);
    add("promptPassword", "string", {"string"}, false, false, true, {}, {0});
    add("flush",          "void",   {});
    add("flushErr",       "void",   {});
    add("isTTY",          "bool",   {});
    add("isStdoutTTY",    "bool",   {});
    add("isStderrTTY",    "bool",   {});

    // ═════════════════════════════════════════════════════════════════════
    // std::math — mathematical functions
    // ═════════════════════════════════════════════════════════════════════

    // Constants
    addConstant("PI",         "float64");
    addConstant("E",          "float64");
    addConstant("TAU",        "float64");
    addConstant("INF",        "float64");
    addConstant("NAN",        "float64");
    addConstant("INT32_MAX",  "int32");
    addConstant("INT32_MIN",  "int32");
    addConstant("INT64_MAX",  "int64");
    addConstant("INT64_MIN",  "int64");
    addConstant("UINT32_MAX", "uint32");
    addConstant("UINT64_MAX", "uint64");

    // Unary float64 → float64
    for (auto& fn : {"sqrt", "cbrt", "exp", "exp2", "ln", "log2", "log10",
                     "sin", "cos", "tan", "asin", "acos", "atan",
                     "sinh", "cosh", "tanh",
                     "ceil", "floor", "round", "trunc",
                     "toRadians", "toDegrees"}) {
        add(fn, "float64", {"float64"});
    }

    // Binary float64 → float64
    add("pow",      "float64", {"float64", "float64"});
    add("hypot",    "float64", {"float64", "float64"});
    add("atan2",    "float64", {"float64", "float64"});

    // Interpolation
    add("lerp",     "float64", {"float64", "float64", "float64"});
    add("map",      "float64", {"float64", "float64", "float64", "float64", "float64"});

    // Checks
    add("isNaN",    "bool", {"float64"});
    add("isInf",    "bool", {"float64"});
    add("isFinite", "bool", {"float64"});

    // Polymorphic numeric
    add("abs",      "_numeric", {"_numeric"}, true);
    add("min",      "_numeric", {"_numeric", "_numeric"}, true);
    add("max",      "_numeric", {"_numeric", "_numeric"}, true);
    add("clamp",    "_numeric", {"_numeric", "_numeric", "_numeric"}, true);

    // ═════════════════════════════════════════════════════════════════════
    // std::str — string manipulation
    // ═════════════════════════════════════════════════════════════════════

    // Search (string, string) → bool/int
    add("contains",    "bool",   {"string", "string"});
    add("startsWith",  "bool",   {"string", "string"});
    add("endsWith",    "bool",   {"string", "string"});
    add("indexOf",     "int64",  {"string", "string"});
    add("lastIndexOf", "int64",  {"string", "string"});
    add("count",       "usize",  {"string", "string"});

    // Transform (string) → string
    add("toUpper",    "_any", {"_any"}, true);
    add("toLower",    "_any", {"_any"}, true);
    add("trim",       "string", {"string"}, false, false, true, {}, {0});
    add("trimLeft",   "string", {"string"}, false, false, true, {}, {0});
    add("trimRight",  "string", {"string"}, false, false, true, {}, {0});
    add("reverse",    "string", {"string"}, false, false, true, {}, {0});

    // Transform (string, string, string) → string
    add("replace",      "string", {"string", "string", "string"}, false, false, true, {}, {0,1,2});
    add("replaceFirst", "string", {"string", "string", "string"}, false, false, true, {}, {0,1,2});

    // Repeat / Pad
    add("repeat",    "string", {"string", "usize"}, false, false, true, {}, {0});
    add("padLeft",   "string", {"string", "usize", "char"});
    add("padRight",  "string", {"string", "usize", "char"});

    // Extraction
    add("substring", "string", {"string", "usize", "usize"}, false, false, true, {}, {0});
    add("charAt",    "char",   {"string", "usize"});
    add("slice",     "string", {"string", "int64", "int64"}, false, false, true, {}, {0});

    // Parsing
    add("parseInt",      "int64",  {"string"});
    add("parseIntRadix", "int64",  {"string", "uint32"});
    add("parseFloat",    "float64", {"string"});

    // Splitting & Joining
    add("split",        "Vec<string>", {"string", "string"});
    add("splitN",       "Vec<string>", {"string", "string", "usize"});
    add("joinVec",      "string",      {"Vec<string>", "string"}, false, false, true, {}, {0,1});
    add("lines",        "Vec<string>", {"string"});
    add("chars",        "Vec<char>",   {"string"});
    add("fromChars",    "string",      {"Vec<char>"}, false, false, true, {}, {0});
    add("toBytes",      "Vec<uint8>",  {"string"});
    add("fromBytes",    "string",      {"Vec<uint8>"}, false, false, true, {}, {0});

    // Conversion
    add("fromCharCode",  "char",   {"int32"});

    // ═════════════════════════════════════════════════════════════════════
    // std::mem — memory management
    // ═════════════════════════════════════════════════════════════════════
    add("alloc",       "*void", {"usize"});
    add("allocZeroed", "*void", {"usize"});
    add("realloc",     "*void", {"*void", "usize"});
    add("free",        "void",  {"*void"});
    add("copy",        "void",  {"*void", "*void", "usize"});
    add("move",        "void",  {"*void", "*void", "usize"});
    add("set",         "void",  {"*void", "uint8", "usize"});
    add("zero",        "void",  {"*void", "usize"});
    add("compare",     "int32", {"*void", "*void", "usize"});

    // ═════════════════════════════════════════════════════════════════════
    // std::random
    // ═════════════════════════════════════════════════════════════════════
    add("seed",           "void",    {"uint64"});
    add("seedTime",       "void",    {});
    add("randInt",        "int64",   {});
    add("randIntRange",   "int64",   {"int64", "int64"});
    add("randUint",       "uint64",  {});
    add("randFloat",      "float64", {});
    add("randFloatRange", "float64", {"float64", "float64"});
    add("randBool",       "bool",    {});
    add("randChar",       "char",    {});
    add("uuid_v4",        "string",  {});

    // ═════════════════════════════════════════════════════════════════════
    // std::time
    // ═════════════════════════════════════════════════════════════════════
    add("now",         "uint64", {});
    add("nowNanos",    "uint64", {});
    add("nowMicros",   "uint64", {});
    add("sleep",       "void",   {"uint64"});
    add("sleepMicros", "void",   {"uint64"});
    add("clock",       "uint64", {});
    add("year",        "int32",  {});
    add("month",       "int32",  {});
    add("day",         "int32",  {});
    add("hour",        "int32",  {});
    add("minute",      "int32",  {});
    add("second",      "int32",  {});
    add("weekday",     "int32",  {});
    add("timestamp",   "string", {});
    add("elapsed",     "uint64", {"uint64"});
    add("formatTime",  "string", {"uint64", "string"});
    add("parseTime",   "uint64", {"string", "string"});

    // ═════════════════════════════════════════════════════════════════════
    // std::fs — file system
    // ═════════════════════════════════════════════════════════════════════
    add("readFile",    "string", {"string"});
    add("writeFile",   "void",   {"string", "string"});
    add("appendFile",  "void",   {"string", "string"});
    add("exists",      "bool",   {"string"});
    add("isFile",      "bool",   {"string"});
    add("isDir",       "bool",   {"string"});
    add("remove",      "bool",   {"string"});
    add("removeDir",   "bool",   {"string"});
    add("rename",      "bool",   {"string", "string"});
    add("mkdir",       "bool",   {"string"});
    add("mkdirAll",    "bool",   {"string"});
    add("listDir",     "Vec<string>", {"string"});
    add("readBytes",    "Vec<uint8>",  {"string"});
    add("writeBytes",   "void",        {"string", "Vec<uint8>"});
    add("fileSize",    "int64",  {"string"});
    add("cwd",         "string", {});
    add("setCwd",      "bool",   {"string"});
    add("tempDir",     "string", {});

    // ═════════════════════════════════════════════════════════════════════
    // std::process
    // ═════════════════════════════════════════════════════════════════════
    add("exit",            "void",   {"int32"});
    add("abort",           "void",   {});
    add("env",             "string", {"string"});
    add("setEnv",          "void",   {"string", "string"});
    add("hasEnv",          "bool",   {"string"});
    add("exec",            "int32",  {"string"});
    add("execOutput",      "string", {"string"});
    add("pid",             "int32",  {});
    add("platform",        "string", {});
    add("arch",            "string", {});
    add("homeDir",         "string", {});
    add("executablePath",  "string", {});

    // ═════════════════════════════════════════════════════════════════════
    // std::conv — type conversions
    // ═════════════════════════════════════════════════════════════════════
    add("itoa",           "string", {"int64"});
    add("itoaRadix",      "string", {"int64", "uint32"});
    add("utoa",           "string", {"uint64"});
    add("ftoa",           "string", {"float64"});
    add("ftoaPrecision",  "string", {"float64", "uint32"});
    add("atoi",           "int64",  {"string"});
    add("atof",           "float64", {"string"});
    add("toHex",          "string", {"uint64"});
    add("toOctal",        "string", {"uint64"});
    add("toBinary",       "string", {"uint64"});
    add("fromHex",        "uint64", {"string"});
    add("charToInt",      "int32",  {"char"});
    add("intToChar",      "char",   {"int32"});

    // ═════════════════════════════════════════════════════════════════════
    // std::hash
    // ═════════════════════════════════════════════════════════════════════
    add("hashString",  "uint64", {"string"});
    add("hashInt",     "uint64", {"int64"});
    add("hashCombine", "uint64", {"uint64", "uint64"});
    add("hashBytes",   "uint64", {"Vec<uint8>"});
    add("crc32",       "uint32", {"Vec<uint8>"});

    // ═════════════════════════════════════════════════════════════════════
    // std::bits — bit manipulation
    // ═════════════════════════════════════════════════════════════════════
    add("popcount",    "uint32", {"uint64"});
    add("ctz",         "uint32", {"uint64"});
    add("clz",         "uint32", {"uint64"});
    add("rotl",        "uint64", {"uint64", "uint32"});
    add("rotr",        "uint64", {"uint64", "uint32"});
    add("bswap",       "uint64", {"uint64"});
    add("bitReverse",  "uint64", {"uint64"});
    add("isPow2",      "bool",   {"uint64"});
    add("nextPow2",    "uint64", {"uint64"});
    add("extractBits", "uint64", {"uint64", "uint32", "uint32"});
    add("setBit",      "uint64", {"uint64", "uint32"});
    add("clearBit",    "uint64", {"uint64", "uint32"});
    add("toggleBit",   "uint64", {"uint64", "uint32"});
    add("testBit",     "bool",   {"uint64", "uint32"});

    // ═════════════════════════════════════════════════════════════════════
    // std::ascii — character classification
    // ═════════════════════════════════════════════════════════════════════
    for (auto& fn : {"isAlpha", "isDigit", "isAlphaNum", "isUpper", "isLower",
                     "isWhitespace", "isPrintable", "isControl", "isHexDigit",
                     "isAscii"}) {
        add(fn, "bool", {"char"});
    }
    // Note: toUpper/toLower overloaded with std::str — ascii versions take char
    // They are already registered under std::str as string→string.
    // The checker will match based on import context.
    add("toDigit",   "int32", {"char"});
    add("fromDigit", "char",  {"int32"});

    // ═════════════════════════════════════════════════════════════════════
    // std::path
    // ═════════════════════════════════════════════════════════════════════
    add("join",          "string", {"string", "string"});
    add("parent",        "string", {"string"});
    add("fileName",      "string", {"string"});
    add("stem",          "string", {"string"});
    add("extension",     "string", {"string"});
    add("isAbsolute",    "bool",   {"string"});
    add("isRelative",    "bool",   {"string"});
    add("normalize",     "string", {"string"});
    add("toAbsolute",    "string", {"string"});
    add("separator",     "char",   {});
    add("withExtension", "string", {"string", "string"});
    add("withFileName",  "string", {"string", "string"});
    add("joinAll",       "string", {"Vec<string>"});

    // ═════════════════════════════════════════════════════════════════════
    // std::fmt — formatting
    // ═════════════════════════════════════════════════════════════════════
    add("lpad",        "string", {"string", "usize", "char"});
    add("rpad",        "string", {"string", "usize", "char"});
    add("center",      "string", {"string", "usize", "char"});
    add("hex",         "string", {"uint64"});
    add("hexUpper",    "string", {"uint64"});
    add("oct",         "string", {"uint64"});
    add("bin",         "string", {"uint64"});
    add("fixed",       "string", {"float64", "uint32"});
    add("scientific",  "string", {"float64"});
    add("humanBytes",  "string", {"uint64"});
    add("commas",      "string", {"int64"});
    add("percent",     "string", {"float64"});

    // ═════════════════════════════════════════════════════════════════════
    // std::regex
    // ═════════════════════════════════════════════════════════════════════
    add("match",             "bool",   {"string", "string"});
    add("find",              "string", {"string", "string"});
    add("findIndex",         "int64",  {"string", "string"});
    add("regexReplace",      "string", {"string", "string", "string"});
    add("regexReplaceFirst", "string", {"string", "string", "string"});
    add("isValid",           "bool",   {"string"});
    add("findAll",           "Vec<string>", {"string", "string"});
    add("regexSplit",        "Vec<string>", {"string", "string"});

    // ═════════════════════════════════════════════════════════════════════
    // std::encoding
    // ═════════════════════════════════════════════════════════════════════
    add("base64EncodeStr", "string", {"string"});
    add("base64DecodeStr", "string", {"string"});
    add("urlEncode",       "string", {"string"});
    add("urlDecode",       "string", {"string"});
    add("base64Encode",    "string",      {"Vec<uint8>"});
    add("base64Decode",    "Vec<uint8>",  {"string"});

    // ═════════════════════════════════════════════════════════════════════
    // std::os
    // ═════════════════════════════════════════════════════════════════════
    add("getpid",    "int32",  {});
    add("getppid",   "int32",  {});
    add("getuid",    "uint32", {});
    add("getgid",    "uint32", {});
    add("hostname",  "string", {});
    add("pageSize",  "usize",  {});
    add("errno",     "int32",  {});
    add("strerror",  "string", {"int32"});
    add("kill",      "int32",  {"int32", "int32"});
    add("dup",       "int32",  {"int32"});
    add("dup2",      "int32",  {"int32", "int32"});
    add("closeFd",   "int32",  {"int32"});

    // ═════════════════════════════════════════════════════════════════════
    // std::test — assertion functions (polymorphic comparisons)
    // ═════════════════════════════════════════════════════════════════════
    add("assertEqual",    "void", {"_any", "_any"}, true);
    add("assertNotEqual", "void", {"_any", "_any"}, true);
    add("assertTrue",     "void", {"bool"});
    add("assertFalse",    "void", {"bool"});
    add("assertGreater",  "void", {"_numeric", "_numeric"}, true);
    add("assertLess",     "void", {"_numeric", "_numeric"}, true);
    add("assertGreaterEq","void", {"_numeric", "_numeric"}, true);
    add("assertLessEq",   "void", {"_numeric", "_numeric"}, true);
    add("assertStringContains", "void", {"string", "string"});
    add("assertNear",     "void", {"float64", "float64", "float64"});
    add("fail",           "void", {"string"});
    add("skip",           "void", {"string"});
    add("log",            "void", {"string"});

    // ═════════════════════════════════════════════════════════════════════
    // std::crypto
    // ═════════════════════════════════════════════════════════════════════
    add("md5String",    "string", {"string"});
    add("sha1String",   "string", {"string"});
    add("sha256String", "string", {"string"});
    add("sha512String", "string", {"string"});
    add("md5",          "string",      {"Vec<uint8>"});
    add("sha1",         "string",      {"Vec<uint8>"});
    add("sha256",       "string",      {"Vec<uint8>"});
    add("sha512",       "string",      {"Vec<uint8>"});
    add("hmacSha256",   "Vec<uint8>",  {"Vec<uint8>", "Vec<uint8>"});
    add("randomBytes",  "Vec<uint8>",  {"usize"});

    // ═════════════════════════════════════════════════════════════════════
    // std::compress
    // ═════════════════════════════════════════════════════════════════════
    add("gzipCompress",   "string", {"string"});
    add("gzipDecompress", "string", {"string"});
    add("deflate",        "string", {"string"});
    add("inflate",        "string", {"string"});
    add("compressLevel",  "string", {"string", "int32"});

    // ═════════════════════════════════════════════════════════════════════
    // std::net — networking
    // ═════════════════════════════════════════════════════════════════════
    add("tcpConnect",  "int32",  {"string", "uint16"});
    add("tcpListen",   "int32",  {"string", "uint16"});
    add("tcpAccept",   "int32",  {"int32"});
    add("tcpSend",     "int64",  {"int32", "string"});
    add("tcpRecv",     "string", {"int32", "usize"});
    add("udpBind",     "int32",  {"string", "uint16"});
    add("udpSendTo",   "int64",  {"int32", "string", "string", "uint16"});
    add("udpRecvFrom", "string", {"int32", "usize"});
    add("close",       "void",   {"int32"});
    add("setTimeout",  "void",   {"int32", "uint64"});
    add("resolve",     "string", {"string"});

    // ═════════════════════════════════════════════════════════════════════
    // std::thread — utilities only (Task/Mutex are types, not functions)
    // ═════════════════════════════════════════════════════════════════════
    add("cpuCount",  "uint32", {});
    add("threadId",  "uint64", {});
    add("yield",     "void",   {});

    // ═════════════════════════════════════════════════════════════════════
    // Global builtins (always available, no import needed)
    // ═════════════════════════════════════════════════════════════════════
    add("panic",         "void",    {"string"});
    add("assert",        "void",    {"bool"});
    add("assertMsg",     "void",    {"bool", "string"});
    add("unreachable",   "void",    {});
    add("toInt",         "int64",   {"string"});
    add("toFloat",       "float64", {"string"});
    add("toBool",        "bool",    {"string"});
    add("toString",      "string",  {"_any"}, true);
    add("cstr",          "*char",   {"string"},
        false, false, false, {}, {},
        "Allocate and return a null-terminated C copy of a Lux string. "
        "The caller **owns** the returned `*char` and must `free()` it when done.");
    add("fromCStr",      "string",  {"*char"},
        false, false, false, {}, {0},
        "Create a string from a null-terminated C string **without copying**. "
        "The returned string **borrows** the pointer — do not free the original "
        "while the string is in use. Use `fromCStrCopy` instead if you need an "
        "owned copy.");
    add("fromCStrCopy",  "string",  {"*char"},
        false, false, true, {}, {},
        "Create a string by copying a null-terminated C string. "
        "The returned string **owns** its memory and will be freed automatically.");
    add("fromCStrLen",   "string",  {"*char", "usize"},
        false, false, false, {}, {0},
        "Create a string from a C string with explicit length, **without copying**. "
        "The returned string **borrows** the pointer — do not free the original "
        "while the string is in use. Use `fromCStrCopy` instead if you need an "
        "owned copy.");
    add("freeStr",       "void",    {"string"}, false, false, false, {0},
        {}, "Free the underlying buffer of a Lux string. "
        "Only needed for strings whose memory must be reclaimed before scope exit. "
        "Calling `freeStr` on a borrowed string (literal, `fromCStr`, `fromCStrLen`) "
        "is undefined behaviour.");
}
