#pragma once

#include <string>
#include <vector>

// Represents a value computed at compile time.
// Analogous to a runtime value, but lives entirely within the compiler process.
class ComptimeValue {
public:
    enum class Kind {
        Void,
        Int,
        Float,
        Bool,
        String,
        Type,     // const TypeInfo*
    };

    static ComptimeValue voidVal();
    static ComptimeValue intVal(int64_t v);
    static ComptimeValue floatVal(double v);
    static ComptimeValue boolVal(bool v);
    static ComptimeValue stringVal(const std::string& v);
    static ComptimeValue typeVal(void* typePtr); // TypeInfo* opaque

    Kind kind() const { return kind_; }

    int64_t      asInt() const;
    double       asFloat() const;
    bool         asBool() const;
    std::string  asString() const;
    void*        asType() const;

private:
    Kind kind_ = Kind::Void;
    int64_t      intVal_ = 0;
    double       floatVal_ = 0.0;
    bool         boolVal_ = false;
    std::string  stringVal_;
    void*        typeVal_ = nullptr; // const TypeInfo*
};
