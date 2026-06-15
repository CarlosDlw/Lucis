#include "imports/ImportResolver.h"

#include <cstdlib>
#include <iostream>

// Known builtin modules → exported symbols
const std::unordered_map<std::string,
                         std::unordered_set<std::string>>
ImportResolver::knownModules_ = {
    { "std::log", { "println", "print", "eprint", "eprintln", "dbg", "sprintf" } },
    { "std::io", { "readLine", "readChar", "readInt", "readFloat", "readBool",
                   "readAll", "prompt", "promptInt", "promptFloat", "promptBool",
                   "readByte", "readLines", "readNBytes", "isEOF",
                   "readPassword", "promptPassword",
                   "flush", "flushErr",
                   "isTTY", "isStdoutTTY", "isStderrTTY" } },
    { "std::math", { "PI", "E", "TAU", "INF", "NAN",
                     "INT32_MAX", "INT32_MIN", "INT64_MAX", "INT64_MIN",
                     "UINT32_MAX", "UINT64_MAX",
                     "abs", "min", "max", "clamp",
                     "sqrt", "cbrt", "pow", "hypot",
                     "exp", "exp2", "ln", "log2", "log10",
                     "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
                     "sinh", "cosh", "tanh",
                     "ceil", "floor", "round", "trunc",
                     "lerp", "map", "toRadians", "toDegrees",
                     "isNaN", "isInf", "isFinite" } },
    { "std::str", { "contains", "startsWith", "endsWith",
                    "indexOf", "lastIndexOf", "count",
                    "toUpper", "toLower",
                    "trim", "trimLeft", "trimRight",
                    "replace", "replaceFirst",
                    "repeat", "reverse",
                    "padLeft", "padRight",
                    "substring", "charAt", "slice",
                    "parseInt", "parseIntRadix", "parseFloat",
                    "fromCharCode",
                    "split", "splitN", "joinVec", "lines",
                    "chars", "fromChars", "toBytes", "fromBytes" } },
    { "std::mem", { "alloc", "allocZeroed", "realloc", "free",
                    "copy", "move", "set", "zero", "compare" } },
    { "std::random", { "seed", "seedTime",
                       "randInt", "randIntRange",
                       "randUint", "randFloat", "randFloatRange",
                       "randBool", "randChar", "uuid_v4" } },
    { "std::time", { "now", "nowNanos", "nowMicros",
                     "sleep", "sleepMicros", "clock",
                     "year", "month", "day",
                     "hour", "minute", "second", "weekday",
                     "timestamp", "elapsed",
                     "formatTime", "parseTime" } },
    { "std::fs", { "readFile", "writeFile", "appendFile",
                   "exists", "isFile", "isDir",
                   "remove", "removeDir", "rename",
                   "mkdir", "mkdirAll",
                   "listDir", "readBytes", "writeBytes",
                   "fileSize", "cwd", "setCwd", "tempDir" } },
    { "std::process", { "exit", "abort",
                        "env", "setEnv", "hasEnv",
                        "exec", "execOutput",
                        "pid", "platform", "arch",
                        "homeDir", "executablePath" } },
    { "std::conv", { "itoa", "itoaRadix", "utoa",
                     "ftoa", "ftoaPrecision",
                     "atoi", "atof",
                     "toHex", "toOctal", "toBinary", "fromHex",
                     "charToInt", "intToChar" } },
    { "std::hash", { "hashString", "hashInt", "hashCombine",
                     "hashBytes", "crc32" } },
    { "std::bits", { "popcount", "ctz", "clz",
                     "rotl", "rotr", "bswap",
                     "isPow2", "nextPow2", "bitReverse",
                     "extractBits", "setBit", "clearBit",
                     "toggleBit", "testBit" } },
    { "std::ascii", { "isAlpha", "isDigit", "isAlphaNum",
                     "isUpper", "isLower", "isWhitespace",
                     "isPrintable", "isControl", "isHexDigit",
                     "isAscii",
                     "toUpper", "toLower",
                     "toDigit", "fromDigit" } },
    { "std::path", { "join", "parent", "fileName", "stem",
                    "extension", "isAbsolute", "isRelative",
                    "normalize", "toAbsolute", "separator",
                    "withExtension", "withFileName", "joinAll" } },
    { "std::fmt", { "lpad", "rpad", "center",
                   "hex", "hexUpper", "oct", "bin",
                   "fixed", "scientific",
                   "humanBytes", "commas", "percent" } },
    { "std::regex", { "match", "find", "findIndex",
                     "regexReplace", "regexReplaceFirst",
                     "isValid", "findAll", "regexSplit" } },
    { "std::encoding", { "base64EncodeStr", "base64DecodeStr",
                        "urlEncode", "urlDecode",
                        "base64Encode", "base64Decode" } },
    { "std::os", { "getpid", "getppid", "getuid", "getgid",
                  "hostname", "pageSize",
                  "errno", "strerror",
                  "kill",
                  "dup", "dup2", "closeFd" } },
    { "std::test", { "assertEqual", "assertNotEqual",
                    "assertTrue", "assertFalse",
                    "assertGreater", "assertLess",
                    "assertGreaterEq", "assertLessEq",
                    "assertStringContains", "assertNear",
                    "fail", "skip", "log" } },
    { "std::crypto", { "md5String", "sha1String",
                      "sha256String", "sha512String",
                      "md5", "sha1", "sha256", "sha512",
                      "hmacSha256", "randomBytes" } },
    { "std::compress", { "gzipCompress", "gzipDecompress",
                        "deflate", "inflate", "compressLevel" } },
    { "std::net", { "tcpConnect", "tcpListen", "tcpAccept",
                   "tcpSend", "tcpRecv",
                   "udpBind", "udpSendTo", "udpRecvFrom",
                   "close", "setTimeout", "resolve" } },
    { "std::thread", { "cpuCount", "threadId", "yield", "Task", "Mutex" } },
};

std::string ImportResolver::suggestImport(const std::string& symbol) {
    for (auto& [mod, syms] : knownModules_) {
        if (syms.count(symbol))
            return "use " + mod + "::" + symbol + ";";
    }
    return "";
}

bool ImportResolver::isStdModule(const std::string& modulePath) {
    // Check exact match against known modules (old :: format)
    if (knownModules_.find(modulePath) != knownModules_.end()) return true;
    // Check path format (std/ prefix)
    if (modulePath.rfind("std/", 0) == 0) return true;
    // Check :: format prefix (std:: prefix)
    if (modulePath.rfind("std::", 0) == 0) return true;
    return false;
}

std::string ImportResolver::stdlibPath() {
    const char* env = std::getenv("LUCIS_STDLIB_DIR");
    if (env) return env;
#ifdef LUCIS_STDLIB_DIR
    return LUCIS_STDLIB_DIR;
#else
    return "/usr/local/share/lucis/stdlib";
#endif
}

bool ImportResolver::moduleExportsSymbol(const std::string& modulePath,
                                         const std::string& symbolName) {
    auto it = knownModules_.find(modulePath);
    return it != knownModules_.end() && it->second.count(symbolName);
}

void ImportResolver::addImport(const std::string& modulePath,
                               const std::string& symbol) {
    if (symbols_.insert(symbol).second) {
        imports_.push_back({ modulePath, symbol });
    }
}

bool ImportResolver::isImported(const std::string& symbol) const {
    return symbols_.count(symbol) > 0;
}

std::string ImportResolver::resolve(const std::string& symbol,
                                    const std::string& typeSuffix) const {
    if (!isImported(symbol)) return "";

    // Map symbol + type suffix → C function name
    // Convention: lucis_<symbol>_<typeSuffix>
    return "lucis_" + symbol + "_" + typeSuffix;
}
