#include "types/MethodRegistry.h"

// ─────────────────────────────────────────────────────────────────────────────
// Key helpers
// ─────────────────────────────────────────────────────────────────────────────

std::string MethodRegistry::kindToString(TypeKind kind) {
    switch (kind) {
        case TypeKind::Integer:  return "Integer";
        case TypeKind::Float:    return "Float";
        case TypeKind::Bool:     return "Bool";
        case TypeKind::Char:     return "Char";
        case TypeKind::String:   return "String";
        case TypeKind::Void:     return "Void";
        case TypeKind::Struct:   return "Struct";
        case TypeKind::Enum:     return "Enum";
        case TypeKind::Pointer:  return "Pointer";
        case TypeKind::Function: return "Function";
        case TypeKind::Extended: return "Extended";
    }
    return "Unknown";
}

std::string MethodRegistry::makeKey(TypeKind kind, const std::string& name) {
    return kindToString(kind) + "::" + name;
}

// ─────────────────────────────────────────────────────────────────────────────
// Registration & lookup
// ─────────────────────────────────────────────────────────────────────────────

MethodRegistry::MethodRegistry() {
    registerBuiltins();
}

void MethodRegistry::registerMethod(MethodDescriptor desc) {
    for (auto kind : desc.receiverKinds) {
        auto key = makeKey(kind, desc.name);
        methods_.emplace(key, desc);
    }
}

void MethodRegistry::registerArrayMethod(MethodDescriptor desc) {
    desc.isArrayMethod = true;
    arrayMethods_.emplace(desc.name, desc);
}

const MethodDescriptor* MethodRegistry::lookup(TypeKind receiverKind,
                                                const std::string& methodName) const {
    auto key = makeKey(receiverKind, methodName);
    auto it = methods_.find(key);
    return it != methods_.end() ? &it->second : nullptr;
}

const MethodDescriptor* MethodRegistry::lookupArrayMethod(
    const std::string& methodName) const {
    auto it = arrayMethods_.find(methodName);
    return it != arrayMethods_.end() ? &it->second : nullptr;
}

std::vector<const MethodDescriptor*> MethodRegistry::methodsFor(TypeKind kind) const {
    std::vector<const MethodDescriptor*> result;
    auto prefix = kindToString(kind) + "::";
    for (auto& [key, desc] : methods_) {
        if (key.substr(0, prefix.size()) == prefix)
            result.push_back(&desc);
    }
    return result;
}

std::vector<const MethodDescriptor*> MethodRegistry::arrayMethods() const {
    std::vector<const MethodDescriptor*> result;
    for (auto& [key, desc] : arrayMethods_)
        result.push_back(&desc);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Built-in method registration
//
// Convention for paramTypes / returnType:
//   "_self"   → same type as the receiver
//   "_elem"   → element type of the array (for []T methods)
//   "uint32"  → literal type name from TypeRegistry
//
// emitTag format: "category.method" — used by IRGen to dispatch codegen
// ─────────────────────────────────────────────────────────────────────────────

void MethodRegistry::registerBuiltins() {

    // ═══════════════════════════════════════════════════════════════════════
    // int* / uint* methods
    // ═══════════════════════════════════════════════════════════════════════

    registerMethod({ "abs",  {TypeKind::Integer}, {}, "_self",
                     .requireSigned = true, .emitTag = "int.abs" });

    registerMethod({ "clamp", {TypeKind::Integer}, {"_self", "_self"}, "_self",
                     .emitTag = "int.clamp" });

    registerMethod({ "pow", {TypeKind::Integer}, {"uint32"}, "_self",
                     .emitTag = "int.pow" });

    registerMethod({ "min", {TypeKind::Integer}, {"_self"}, "_self",
                     .emitTag = "int.min" });

    registerMethod({ "max", {TypeKind::Integer}, {"_self"}, "_self",
                     .emitTag = "int.max" });

    registerMethod({ "wrappingAdd", {TypeKind::Integer}, {"_self"}, "_self",
                     .emitTag = "int.wrappingAdd" });

    registerMethod({ "wrappingSub", {TypeKind::Integer}, {"_self"}, "_self",
                     .emitTag = "int.wrappingSub" });

    registerMethod({ "wrappingMul", {TypeKind::Integer}, {"_self"}, "_self",
                     .emitTag = "int.wrappingMul" });

    registerMethod({ "saturatingAdd", {TypeKind::Integer}, {"_self"}, "_self",
                     .emitTag = "int.saturatingAdd" });

    registerMethod({ "saturatingSub", {TypeKind::Integer}, {"_self"}, "_self",
                     .emitTag = "int.saturatingSub" });

    registerMethod({ "leadingZeros", {TypeKind::Integer}, {}, "uint32",
                     .emitTag = "int.leadingZeros" });

    registerMethod({ "trailingZeros", {TypeKind::Integer}, {}, "uint32",
                     .emitTag = "int.trailingZeros" });

    registerMethod({ "countOnes", {TypeKind::Integer}, {}, "uint32",
                     .emitTag = "int.countOnes" });

    registerMethod({ "rotateLeft", {TypeKind::Integer}, {"uint32"}, "_self",
                     .emitTag = "int.rotateLeft" });

    registerMethod({ "rotateRight", {TypeKind::Integer}, {"uint32"}, "_self",
                     .emitTag = "int.rotateRight" });

    registerMethod({ "toBigEndian", {TypeKind::Integer}, {}, "_self",
                     .emitTag = "int.toBigEndian" });

    registerMethod({ "toLittleEndian", {TypeKind::Integer}, {}, "_self",
                     .emitTag = "int.toLittleEndian" });

    registerMethod({ "byteSwap", {TypeKind::Integer}, {}, "_self",
                     .emitTag = "int.byteSwap" });

    registerMethod({ "isPowerOfTwo", {TypeKind::Integer}, {}, "bool",
                     .emitTag = "int.isPowerOfTwo" });

    registerMethod({ "nextPowerOfTwo", {TypeKind::Integer}, {}, "_self",
                     .emitTag = "int.nextPowerOfTwo" });

    registerMethod({ "log2", {TypeKind::Integer}, {}, "uint32",
                     .emitTag = "int.log2" });

    registerMethod({ "sign", {TypeKind::Integer}, {}, "int32",
                     .requireSigned = true, .emitTag = "int.sign" });

    registerMethod({ "toString", {TypeKind::Integer}, {}, "string",
                     .emitTag = "int.toString" });

    registerMethod({ "toStringRadix", {TypeKind::Integer}, {"uint32"}, "string",
                     .emitTag = "int.toStringRadix" });

    registerMethod({ "toFloat", {TypeKind::Integer}, {}, "float64",
                     .emitTag = "int.toFloat" });

    registerMethod({ "toChar", {TypeKind::Integer}, {}, "char",
                     .emitTag = "int.toChar" });

    // ═══════════════════════════════════════════════════════════════════════
    // float32 / float64 / double methods
    // ═══════════════════════════════════════════════════════════════════════

    registerMethod({ "abs",   {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.abs" });

    registerMethod({ "ceil",  {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.ceil" });

    registerMethod({ "floor", {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.floor" });

    registerMethod({ "round", {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.round" });

    registerMethod({ "trunc", {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.trunc" });

    registerMethod({ "fract", {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.fract" });

    registerMethod({ "sqrt",  {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.sqrt" });

    registerMethod({ "cbrt",  {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.cbrt" });

    registerMethod({ "pow",   {TypeKind::Float}, {"_self"}, "_self",
                     .emitTag = "float.pow" });

    registerMethod({ "exp",   {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.exp" });

    registerMethod({ "exp2",  {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.exp2" });

    registerMethod({ "ln",    {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.ln" });

    registerMethod({ "log2",  {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.log2" });

    registerMethod({ "log10", {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.log10" });

    registerMethod({ "sin",   {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.sin" });

    registerMethod({ "cos",   {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.cos" });

    registerMethod({ "tan",   {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.tan" });

    registerMethod({ "asin",  {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.asin" });

    registerMethod({ "acos",  {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.acos" });

    registerMethod({ "atan",  {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.atan" });

    registerMethod({ "atan2", {TypeKind::Float}, {"_self"}, "_self",
                     .emitTag = "float.atan2" });

    registerMethod({ "sinh",  {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.sinh" });

    registerMethod({ "cosh",  {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.cosh" });

    registerMethod({ "tanh",  {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.tanh" });

    registerMethod({ "hypot", {TypeKind::Float}, {"_self"}, "_self",
                     .emitTag = "float.hypot" });

    registerMethod({ "min",   {TypeKind::Float}, {"_self"}, "_self",
                     .emitTag = "float.min" });

    registerMethod({ "max",   {TypeKind::Float}, {"_self"}, "_self",
                     .emitTag = "float.max" });

    registerMethod({ "clamp", {TypeKind::Float}, {"_self", "_self"}, "_self",
                     .emitTag = "float.clamp" });

    registerMethod({ "lerp",  {TypeKind::Float}, {"_self", "_self"}, "_self",
                     .emitTag = "float.lerp" });

    registerMethod({ "sign",  {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.sign" });

    registerMethod({ "copySign", {TypeKind::Float}, {"_self"}, "_self",
                     .emitTag = "float.copySign" });

    registerMethod({ "isNaN",    {TypeKind::Float}, {}, "bool",
                     .emitTag = "float.isNaN" });

    registerMethod({ "isInf",    {TypeKind::Float}, {}, "bool",
                     .emitTag = "float.isInf" });

    registerMethod({ "isFinite", {TypeKind::Float}, {}, "bool",
                     .emitTag = "float.isFinite" });

    registerMethod({ "isNormal", {TypeKind::Float}, {}, "bool",
                     .emitTag = "float.isNormal" });

    registerMethod({ "isNegative", {TypeKind::Float}, {}, "bool",
                     .emitTag = "float.isNegative" });

    registerMethod({ "toRadians", {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.toRadians" });

    registerMethod({ "toDegrees", {TypeKind::Float}, {}, "_self",
                     .emitTag = "float.toDegrees" });

    registerMethod({ "toString", {TypeKind::Float}, {}, "string",
                     .emitTag = "float.toString" });

    registerMethod({ "toStringPrecision", {TypeKind::Float}, {"uint32"}, "string",
                     .emitTag = "float.toStringPrecision" });

    registerMethod({ "toInt",  {TypeKind::Float}, {}, "int64",
                     .emitTag = "float.toInt" });

    registerMethod({ "toBits", {TypeKind::Float}, {}, "uint64",
                     .emitTag = "float.toBits" });

    // ═══════════════════════════════════════════════════════════════════════
    // bool methods
    // ═══════════════════════════════════════════════════════════════════════

    registerMethod({ "toString", {TypeKind::Bool}, {}, "string",
                     .emitTag = "bool.toString" });

    registerMethod({ "toInt",    {TypeKind::Bool}, {}, "int32",
                     .emitTag = "bool.toInt" });

    registerMethod({ "toggle",   {TypeKind::Bool}, {}, "bool",
                     .emitTag = "bool.toggle" });

    // ═══════════════════════════════════════════════════════════════════════
    // char methods
    // ═══════════════════════════════════════════════════════════════════════

    registerMethod({ "isAlpha",    {TypeKind::Char}, {}, "bool",
                     .emitTag = "char.isAlpha" });

    registerMethod({ "isDigit",    {TypeKind::Char}, {}, "bool",
                     .emitTag = "char.isDigit" });

    registerMethod({ "isHexDigit", {TypeKind::Char}, {}, "bool",
                     .emitTag = "char.isHexDigit" });

    registerMethod({ "isAlphaNum", {TypeKind::Char}, {}, "bool",
                     .emitTag = "char.isAlphaNum" });

    registerMethod({ "isUpper",    {TypeKind::Char}, {}, "bool",
                     .emitTag = "char.isUpper" });

    registerMethod({ "isLower",    {TypeKind::Char}, {}, "bool",
                     .emitTag = "char.isLower" });

    registerMethod({ "isSpace",    {TypeKind::Char}, {}, "bool",
                     .emitTag = "char.isSpace" });

    registerMethod({ "isPrintable",{TypeKind::Char}, {}, "bool",
                     .emitTag = "char.isPrintable" });

    registerMethod({ "isControl",  {TypeKind::Char}, {}, "bool",
                     .emitTag = "char.isControl" });

    registerMethod({ "isPunct",    {TypeKind::Char}, {}, "bool",
                     .emitTag = "char.isPunct" });

    registerMethod({ "isAscii",    {TypeKind::Char}, {}, "bool",
                     .emitTag = "char.isAscii" });

    registerMethod({ "toUpper",    {TypeKind::Char}, {}, "char",
                     .emitTag = "char.toUpper" });

    registerMethod({ "toLower",    {TypeKind::Char}, {}, "char",
                     .emitTag = "char.toLower" });

    registerMethod({ "toInt",      {TypeKind::Char}, {}, "int32",
                     .emitTag = "char.toInt" });

    registerMethod({ "toString",   {TypeKind::Char}, {}, "string",
                     .emitTag = "char.toString" });

    registerMethod({ "repeat",     {TypeKind::Char}, {"usize"}, "string",
                     .emitTag = "char.repeat" });

    registerMethod({ "digitToInt", {TypeKind::Char}, {}, "int32",
                     .emitTag = "char.digitToInt" });

    // ═══════════════════════════════════════════════════════════════════════
    // string methods
    // ═══════════════════════════════════════════════════════════════════════

    registerMethod({ "len",       {TypeKind::String}, {}, "usize",
                     .emitTag = "string.len" });
    
    registerMethod({ "ptr",       {TypeKind::String}, {}, "*void",
                     .emitTag = "string.ptr"  });

    registerMethod({ "isEmpty",   {TypeKind::String}, {}, "bool",
                     .emitTag = "string.isEmpty" });

    registerMethod({ "at",        {TypeKind::String}, {"usize"}, "char",
                     .emitTag = "string.at" });

    registerMethod({ "front",     {TypeKind::String}, {}, "char",
                     .emitTag = "string.front" });

    registerMethod({ "back",      {TypeKind::String}, {}, "char",
                     .emitTag = "string.back" });

    registerMethod({ "contains",  {TypeKind::String}, {"string"}, "bool",
                     .emitTag = "string.contains" });

    registerMethod({ "startsWith",{TypeKind::String}, {"string"}, "bool",
                     .emitTag = "string.startsWith" });

    registerMethod({ "endsWith",  {TypeKind::String}, {"string"}, "bool",
                     .emitTag = "string.endsWith" });

    registerMethod({ "indexOf",   {TypeKind::String}, {"string"}, "int64",
                     .emitTag = "string.indexOf" });

    registerMethod({ "lastIndexOf",{TypeKind::String}, {"string"}, "int64",
                     .emitTag = "string.lastIndexOf" });

    registerMethod({ "count",     {TypeKind::String}, {"string"}, "usize",
                     .emitTag = "string.count" });

    registerMethod({ "substring", {TypeKind::String}, {"usize", "usize"}, "string",
                     .emitTag = "string.substring" });

    registerMethod({ "slice",     {TypeKind::String}, {"int64", "int64"}, "string",
                     .emitTag = "string.slice" });

    registerMethod({ "trim",      {TypeKind::String}, {}, "string",
                     .emitTag = "string.trim" });

    registerMethod({ "trimLeft",  {TypeKind::String}, {}, "string",
                     .emitTag = "string.trimLeft" });

    registerMethod({ "trimRight", {TypeKind::String}, {}, "string",
                     .emitTag = "string.trimRight" });

    registerMethod({ "trimChar",  {TypeKind::String}, {"char"}, "string",
                     .emitTag = "string.trimChar" });

    registerMethod({ "padLeft",   {TypeKind::String}, {"usize", "char"}, "string",
                     .emitTag = "string.padLeft" });

    registerMethod({ "padRight",  {TypeKind::String}, {"usize", "char"}, "string",
                     .emitTag = "string.padRight" });

    registerMethod({ "toUpper",   {TypeKind::String}, {}, "string",
                     .emitTag = "string.toUpper" });

    registerMethod({ "toLower",   {TypeKind::String}, {}, "string",
                     .emitTag = "string.toLower" });

    registerMethod({ "capitalize",{TypeKind::String}, {}, "string",
                     .emitTag = "string.capitalize" });

    registerMethod({ "replace",   {TypeKind::String}, {"string", "string"}, "string",
                     .emitTag = "string.replace" });

    registerMethod({ "replaceFirst",{TypeKind::String}, {"string", "string"}, "string",
                     .emitTag = "string.replaceFirst" });

    registerMethod({ "removePrefix",{TypeKind::String}, {"string"}, "string",
                     .emitTag = "string.removePrefix" });

    registerMethod({ "removeSuffix",{TypeKind::String}, {"string"}, "string",
                     .emitTag = "string.removeSuffix" });

    registerMethod({ "split",     {TypeKind::String}, {"string"}, "Vec<string>",
                     .emitTag = "string.split" });

    registerMethod({ "join",      {TypeKind::String}, {"string"}, "string",
                     .emitTag = "string.join" });

    registerMethod({ "repeat",    {TypeKind::String}, {"usize"}, "string",
                     .emitTag = "string.repeat" });

    registerMethod({ "reverse",   {TypeKind::String}, {}, "string",
                     .emitTag = "string.reverse" });

    registerMethod({ "insert",    {TypeKind::String}, {"usize", "string"}, "string",
                     .emitTag = "string.insert" });

    registerMethod({ "remove",    {TypeKind::String}, {"usize", "usize"}, "string",
                     .emitTag = "string.remove" });

    registerMethod({ "chars",     {TypeKind::String}, {}, "Vec<char>",
                     .emitTag = "string.chars" });

    registerMethod({ "bytes",     {TypeKind::String}, {}, "Vec<uint8>",
                     .emitTag = "string.bytes" });

    registerMethod({ "lines",     {TypeKind::String}, {}, "Vec<string>",
                     .emitTag = "string.lines" });

    registerMethod({ "words",     {TypeKind::String}, {}, "Vec<string>",
                     .emitTag = "string.words" });

    registerMethod({ "concat",    {TypeKind::String}, {"string"}, "string",
                     .emitTag = "string.concat" });

    registerMethod({ "compareTo", {TypeKind::String}, {"string"}, "int32",
                     .emitTag = "string.compareTo" });

    registerMethod({ "equalsIgnoreCase", {TypeKind::String}, {"string"}, "bool",
                     .emitTag = "string.equalsIgnoreCase" });

    registerMethod({ "isNumeric", {TypeKind::String}, {}, "bool",
                     .emitTag = "string.isNumeric" });

    registerMethod({ "isAlpha",   {TypeKind::String}, {}, "bool",
                     .emitTag = "string.isAlpha" });

    registerMethod({ "isAlphaNum",{TypeKind::String}, {}, "bool",
                     .emitTag = "string.isAlphaNum" });

    registerMethod({ "isUpper",   {TypeKind::String}, {}, "bool",
                     .emitTag = "string.isUpper" });

    registerMethod({ "isLower",   {TypeKind::String}, {}, "bool",
                     .emitTag = "string.isLower" });

    registerMethod({ "isBlank",   {TypeKind::String}, {}, "bool",
                     .emitTag = "string.isBlank" });

    registerMethod({ "toInt",     {TypeKind::String}, {}, "int64",
                     .emitTag = "string.toInt" });

    registerMethod({ "toFloat",   {TypeKind::String}, {}, "float64",
                     .emitTag = "string.toFloat" });

    registerMethod({ "toBool",    {TypeKind::String}, {}, "bool",
                     .emitTag = "string.toBool" });

    registerMethod({ "hash",      {TypeKind::String}, {}, "uint64",
                     .emitTag = "string.hash" });

    // ═══════════════════════════════════════════════════════════════════════
    // []T array methods
    //
    // "_elem" in paramTypes/returnType = element type of the array
    // "_self" = the array type itself ([]T)
    // ═══════════════════════════════════════════════════════════════════════

    registerArrayMethod({ "len",        {}, {}, "usize",
                          .emitTag = "array.len" });

    registerArrayMethod({ "isEmpty",    {}, {}, "bool",
                          .emitTag = "array.isEmpty" });

    registerArrayMethod({ "at",         {}, {"usize"}, "_elem",
                          .emitTag = "array.at" });

    registerArrayMethod({ "first",      {}, {}, "_elem",
                          .emitTag = "array.first" });

    registerArrayMethod({ "last",       {}, {}, "_elem",
                          .emitTag = "array.last" });

    registerArrayMethod({ "contains",   {}, {"_elem"}, "bool",
                          .emitTag = "array.contains" });

    registerArrayMethod({ "indexOf",    {}, {"_elem"}, "int64",
                          .emitTag = "array.indexOf" });

    registerArrayMethod({ "lastIndexOf",{}, {"_elem"}, "int64",
                          .emitTag = "array.lastIndexOf" });

    registerArrayMethod({ "count",      {}, {"_elem"}, "usize",
                          .emitTag = "array.count" });

    registerArrayMethod({ "fill",       {}, {"_elem"}, "void",
                          .emitTag = "array.fill" });

    registerArrayMethod({ "swap",       {}, {"usize", "usize"}, "void",
                          .emitTag = "array.swap" });

    registerArrayMethod({ "reverse",    {}, {}, "void",
                          .emitTag = "array.reverse" });

    registerArrayMethod({ "copy",       {}, {}, "_self",
                          .emitTag = "array.copy" });

    registerArrayMethod({ "slice",      {}, {"usize", "usize"}, "_self",
                          .emitTag = "array.slice" });

    registerArrayMethod({ "sum",        {}, {}, "_elem",
                          .requireNumeric = true, .emitTag = "array.sum" });

    registerArrayMethod({ "product",    {}, {}, "_elem",
                          .requireNumeric = true, .emitTag = "array.product" });

    registerArrayMethod({ "min",        {}, {}, "_elem",
                          .requireNumeric = true, .emitTag = "array.min" });

    registerArrayMethod({ "max",        {}, {}, "_elem",
                          .requireNumeric = true, .emitTag = "array.max" });

    registerArrayMethod({ "minIndex",   {}, {}, "usize",
                          .requireNumeric = true, .emitTag = "array.minIndex" });

    registerArrayMethod({ "maxIndex",   {}, {}, "usize",
                          .requireNumeric = true, .emitTag = "array.maxIndex" });

    registerArrayMethod({ "average",    {}, {}, "float64",
                          .requireNumeric = true, .emitTag = "array.average" });

    registerArrayMethod({ "isSorted",   {}, {}, "bool",
                          .requireNumeric = true, .emitTag = "array.isSorted" });

    registerArrayMethod({ "sort",       {}, {}, "void",
                          .requireNumeric = true, .emitTag = "array.sort" });

    registerArrayMethod({ "sortDesc",   {}, {}, "void",
                          .requireNumeric = true, .emitTag = "array.sortDesc" });

    registerArrayMethod({ "equals",     {}, {"_self"}, "bool",
                          .emitTag = "array.equals" });

    registerArrayMethod({ "toString",   {}, {}, "string",
                          .emitTag = "array.toString" });

    registerArrayMethod({ "join",       {}, {"string"}, "string",
                          .emitTag = "array.join" });

    registerArrayMethod({ "rotate",     {}, {"int32"}, "void",
                          .emitTag = "array.rotate" });
}
