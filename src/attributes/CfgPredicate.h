#pragma once

#include <string>
#include <vector>

struct TargetInfo;

struct CfgPredicate {
    enum Type { KeyValue, Call, Ident };

    Type type;
    std::string name;          // key name, call function (all/any/not), or shorthand ident
    std::string stringValue;   // for KeyValue: the value string (e.g. "linux")
    std::vector<CfgPredicate> args; // for Call: nested predicates

    CfgPredicate() : type(Ident) {}

    // Evaluate this predicate against the given target info.
    bool evaluate(const TargetInfo& target) const;

    // Format for error messages.
    std::string toString() const;
};
