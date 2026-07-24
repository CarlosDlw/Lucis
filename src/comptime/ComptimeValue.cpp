#include "comptime/ComptimeValue.h"

ComptimeValue ComptimeValue::voidVal() {
    ComptimeValue v;
    v.kind_ = Kind::Void;
    return v;
}

ComptimeValue ComptimeValue::intVal(int64_t v) {
    ComptimeValue val;
    val.kind_ = Kind::Int;
    val.intVal_ = v;
    return val;
}

ComptimeValue ComptimeValue::floatVal(double v) {
    ComptimeValue val;
    val.kind_ = Kind::Float;
    val.floatVal_ = v;
    return val;
}

ComptimeValue ComptimeValue::boolVal(bool v) {
    ComptimeValue val;
    val.kind_ = Kind::Bool;
    val.boolVal_ = v;
    return val;
}

ComptimeValue ComptimeValue::stringVal(const std::string& v) {
    ComptimeValue val;
    val.kind_ = Kind::String;
    val.stringVal_ = v;
    return val;
}

ComptimeValue ComptimeValue::typeVal(void* typePtr) {
    ComptimeValue val;
    val.kind_ = Kind::Type;
    val.typeVal_ = typePtr;
    return val;
}

ComptimeValue ComptimeValue::listVal(std::vector<ComptimeValue> v) {
    ComptimeValue val;
    val.kind_ = Kind::List;
    val.listVal_ = std::move(v);
    return val;
}

ComptimeValue ComptimeValue::structVal(std::unordered_map<std::string, ComptimeValue> v) {
    ComptimeValue val;
    val.kind_ = Kind::Struct;
    val.structVal_ = std::move(v);
    return val;
}

ComptimeValue ComptimeValue::functionVal(void* funcDecl) {
    ComptimeValue val;
    val.kind_ = Kind::Function;
    val.funcDecl_ = funcDecl;
    return val;
}

int64_t ComptimeValue::asInt() const { return intVal_; }
double ComptimeValue::asFloat() const { return floatVal_; }
bool ComptimeValue::asBool() const { return boolVal_; }
std::string ComptimeValue::asString() const { return stringVal_; }
void* ComptimeValue::asType() const { return typeVal_; }
const std::vector<ComptimeValue>& ComptimeValue::asList() const { return listVal_; }
const std::unordered_map<std::string, ComptimeValue>& ComptimeValue::asStruct() const { return structVal_; }
void* ComptimeValue::asFunction() const { return funcDecl_; }
