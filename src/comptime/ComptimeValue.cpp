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

int64_t ComptimeValue::asInt() const { return intVal_; }
double ComptimeValue::asFloat() const { return floatVal_; }
bool ComptimeValue::asBool() const { return boolVal_; }
std::string ComptimeValue::asString() const { return stringVal_; }
void* ComptimeValue::asType() const { return typeVal_; }
