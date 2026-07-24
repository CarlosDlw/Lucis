#pragma once

#include <string>
#include <vector>
#include <unordered_map>

class ComptimeValue {
public:
    enum class Kind {
        Void,
        Int,
        Float,
        Bool,
        String,
        Type,     // const TypeInfo*
        List,
        Struct,
        Function,
    };

    static ComptimeValue voidVal();
    static ComptimeValue intVal(int64_t v);
    static ComptimeValue floatVal(double v);
    static ComptimeValue boolVal(bool v);
    static ComptimeValue stringVal(const std::string& v);
    static ComptimeValue typeVal(void* typePtr);
    static ComptimeValue listVal(std::vector<ComptimeValue> v);
    static ComptimeValue structVal(std::unordered_map<std::string, ComptimeValue> v);
    static ComptimeValue functionVal(void* funcDecl);

    Kind kind() const { return kind_; }

    int64_t      asInt() const;
    double       asFloat() const;
    bool         asBool() const;
    std::string  asString() const;
    void*        asType() const;
    const std::vector<ComptimeValue>& asList() const;
    const std::unordered_map<std::string, ComptimeValue>& asStruct() const;
    void*        asFunction() const;

private:
    Kind kind_ = Kind::Void;
    int64_t      intVal_ = 0;
    double       floatVal_ = 0.0;
    bool         boolVal_ = false;
    std::string  stringVal_;
    void*        typeVal_ = nullptr;
    std::vector<ComptimeValue> listVal_;
    std::unordered_map<std::string, ComptimeValue> structVal_;
    void*        funcDecl_ = nullptr;
};
