#include "cli/ArgParser.h"
#include <algorithm>
#include <cctype>

ArgParser::ArgParser(const std::string& progName, const std::string& description)
    : progName_(progName), description_(description) {}

void ArgParser::addPositional(const std::string& name, const std::string& help, bool required) {
    ArgDef def;
    def.longName    = name;
    def.help        = help;
    def.isFlag      = true;
    def.isPositional = true;
    def.required    = required;
    positionalDefs_.push_back(def);
}

void ArgParser::addFlag(const std::string& longName, char shortName, const std::string& help) {
    ArgDef def;
    def.longName  = longName;
    def.shortName = shortName;
    def.help      = help;
    def.isFlag    = true;
    optionDefs_.push_back(def);
}

void ArgParser::addOption(const std::string& longName, char shortName,
                           const std::string& metaVar, const std::string& help,
                           bool repeatable) {
    ArgDef def;
    def.longName   = longName;
    def.shortName  = shortName;
    def.help       = help;
    def.metaVar    = metaVar;
    def.isFlag     = false;
    def.repeatable = repeatable;
    optionDefs_.push_back(def);
}

const ArgDef* ArgParser::findDef(const std::string& name) const {
    for (auto& def : optionDefs_) {
        if (def.longName == name ||
            (def.shortName != '\0' && std::string(1, def.shortName) == name))
            return &def;
    }
    for (auto& def : positionalDefs_) {
        if (def.longName == name) return &def;
    }
    return nullptr;
}

bool ArgParser::parse(int argc, char* argv[], std::vector<std::string>* outRemaining) {
    parsed_ = true;
    values_.clear();
    remaining_.clear();
    wantsHelp_ = false;

    // Check for --help or -h anywhere in args (before positional parsing)
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            wantsHelp_ = true;
            printHelp();
            return false;
        }
    }

    // Init repeatable options
    for (auto& def : optionDefs_) {
        if (def.repeatable)
            values_[def.longName] = {};
    }

    size_t posIdx = 0;
    bool afterDoubleDash = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // Everything after -- is remaining
        if (arg == "--") {
            afterDoubleDash = true;
            continue;
        }

        if (afterDoubleDash) {
            remaining_.push_back(arg);
            if (outRemaining) outRemaining->push_back(arg);
            continue;
        }

        // Long flag/option (--xxx)
        if (arg.rfind("--", 0) == 0) {
            std::string name = arg.substr(2);
            std::string value;

            // Check for --name=value form
            auto eqPos = name.find('=');
            if (eqPos != std::string::npos) {
                value = name.substr(eqPos + 1);
                name  = name.substr(0, eqPos);
            }

            auto* def = findDef(name);
            if (!def) {
                printError("unknown flag '" + arg + "'");
                return false;
            }
            if (def->isFlag) {
                if (!value.empty()) {
                    printError("flag '" + arg + "' does not take a value");
                    return false;
                }
                if (def->repeatable)
                    values_[def->longName].push_back("1");
                else
                    values_[def->longName] = {"1"};
            } else {
                if (value.empty()) {
                    if (i + 1 >= argc) {
                        printError("option '" + arg + "' requires a value");
                        return false;
                    }
                    value = argv[++i];
                }
                values_[def->longName].push_back(value);
            }
            continue;
        }

        // Short flag/option (-x)
        if (arg.rfind("-", 0) == 0 && arg.size() > 1) {
            std::string name = arg.substr(1);

            // Support -lxxx (no space) form for linker etc.
            auto* def = findDef(name);
            if (!def && name.size() > 1 && findDef(std::string(1, name[0]))) {
                // -lraylib style: single-char flag with attached value
                char shortName = name[0];
                std::string attachedVal = name.substr(1);
                auto* sdef = findDef(std::string(1, shortName));
                if (sdef && !sdef->isFlag) {
                    values_[sdef->longName].push_back(attachedVal);
                    continue;
                }
            }

            if (!def) {
                printError("unknown flag '" + arg + "'");
                return false;
            }
            if (def->isFlag) {
                if (def->repeatable)
                    values_[def->longName].push_back("1");
                else
                    values_[def->longName] = {"1"};
            } else {
                std::string val;
                if (i + 1 < argc)
                    val = argv[++i];
                else {
                    printError("option '" + arg + "' requires a value");
                    return false;
                }
                values_[def->longName].push_back(val);
            }
            continue;
        }

        // Positional argument
        if (posIdx < positionalDefs_.size()) {
            auto& def = positionalDefs_[posIdx];
            values_[def.longName] = {arg};
            posIdx++;
        } else {
            // Extra positional → remaining
            remaining_.push_back(arg);
            if (outRemaining) outRemaining->push_back(arg);
        }
    }

    // Check required positionals
    for (size_t i = posIdx; i < positionalDefs_.size(); i++) {
        if (positionalDefs_[i].required) {
            printError("missing required argument: " + positionalDefs_[i].longName);
            return false;
        }
    }

    return true;
}

bool ArgParser::has(const std::string& name) const {
    auto it = values_.find(name);
    return it != values_.end() && !it->second.empty();
}

std::string ArgParser::get(const std::string& name) const {
    auto it = values_.find(name);
    if (it != values_.end() && !it->second.empty())
        return it->second[0];
    return {};
}

std::vector<std::string> ArgParser::getAll(const std::string& name) const {
    auto it = values_.find(name);
    if (it != values_.end())
        return it->second;
    return {};
}

int ArgParser::getInt(const std::string& name, int defaultVal) const {
    auto v = get(name);
    if (v.empty()) return defaultVal;
    return std::stoi(v);
}

void ArgParser::printHelp() const {
    std::cout << progName_ << " — " << description_ << "\n\n";

    if (!positionalDefs_.empty()) {
        std::cout << "Arguments:\n";
        for (auto& def : positionalDefs_) {
            std::cout << "  " << def.longName;
            if (!def.required) std::cout << " (optional)";
            std::cout << "\n      " << def.help << "\n";
        }
        std::cout << "\n";
    }

    if (!optionDefs_.empty()) {
        std::cout << "Options:\n";
        for (auto& def : optionDefs_) {
            std::cout << "  ";
            if (def.shortName != '\0')
                std::cout << "-" << def.shortName << ", ";
            else
                std::cout << "    ";
            std::cout << "--" << def.longName;
            if (!def.isFlag)
                std::cout << " <" << def.metaVar << ">";
            if (def.repeatable)
                std::cout << " (repeatable)";
            std::cout << "\n      " << def.help << "\n";
        }
        std::cout << "\n";
    }
}

void ArgParser::printError(const std::string& msg) const {
    std::cerr << "\033[1;31mlux:\033[0m " << msg << "\n";
    std::cerr << "Run 'lux help' for usage.\n";
}
