#include "attributes/CfgPredicate.h"
#include "attributes/TargetInfo.h"

bool CfgPredicate::evaluate(const TargetInfo& target) const {
    switch (type) {
    case KeyValue: {
        // name = "stringValue"  e.g. target_os = "linux"
        auto targetVal = target.get(name);
        if (targetVal.empty())
            return false;
        return targetVal == stringValue;
    }
    case Ident: {
        // Bare identifier: may be a shorthand like "unix", "windows", etc.
        if (name == "unix")
            return target.osFamily == "unix";
        if (name == "windows")
            return target.os == "windows";
        if (name == "x86_64")
            return target.arch == "x86_64";
        if (name == "aarch64")
            return target.arch == "aarch64";
        if (name == "debug")
            return target.debug;
        // Fallback: treat as key that must be truthy (non-empty, not "false")
        auto val = target.get(name);
        if (val.empty())
            return false;
        return val != "false" && val != "0";
    }
    case Call: {
        if (name == "all") {
            for (const auto& a : args)
                if (!a.evaluate(target))
                    return false;
            return true;
        }
        if (name == "any") {
            for (const auto& a : args)
                if (a.evaluate(target))
                    return true;
            return false;
        }
        if (name == "not") {
            if (args.size() != 1)
                return false;
            return !args[0].evaluate(target);
        }
        // Unknown call name → false (safe default)
        return false;
    }
    }
    return false;
}

std::string CfgPredicate::toString() const {
    switch (type) {
    case KeyValue:
        return name + " = \"" + stringValue + "\"";
    case Ident:
        return name;
    case Call: {
        std::string r = name + "(";
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) r += ", ";
            r += args[i].toString();
        }
        return r + ")";
    }
    }
    return {};
}
