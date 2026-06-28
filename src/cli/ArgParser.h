#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <iostream>

struct ArgDef {
    std::string longName;
    char shortName = '\0';
    std::string help;
    std::string metaVar;       // for options: e.g. "FILE"
    bool isFlag     = true;    // true = no value, false = takes value
    bool isPositional = false;
    bool required   = false;
    bool repeatable = false;   // option can appear multiple times
    bool isSection  = false;   // section header (not a real option)
};

class ArgParser {
public:
    ArgParser(const std::string& progName, const std::string& description);

    // Define arguments
    void addPositional(const std::string& name, const std::string& help, bool required = true);
    void addFlag(const std::string& longName, char shortName, const std::string& help);
    void addOption(const std::string& longName, char shortName,
                   const std::string& metaVar, const std::string& help,
                   bool repeatable = false);
    void addSection(const std::string& title);  // section header in help output

    // Parse argv. Returns false on error or --help (prints error/help message).
    bool parse(int argc, char* argv[], std::vector<std::string>* outRemaining = nullptr);

    // Returns true if --help or -h was passed
    bool wantsHelp() const { return wantsHelp_; }

    // Access parsed values
    bool has(const std::string& name) const;
    std::string get(const std::string& name) const;
    std::vector<std::string> getAll(const std::string& name) const;
    int getInt(const std::string& name, int defaultVal = 0) const;

    // Remaining positional args (after -- or extra positionals)
    const std::vector<std::string>& remaining() const { return remaining_; }

    // Print help to stdout
    void printHelp() const;

    // Error printing
    void printError(const std::string& msg) const;

private:
    std::string progName_;
    std::string description_;
    std::vector<ArgDef> positionalDefs_;
    std::vector<ArgDef> optionDefs_;
    std::map<std::string, std::vector<std::string>> values_;
    std::vector<std::string> remaining_;
    bool parsed_ = false;
    bool wantsHelp_ = false;

    const ArgDef* findDef(const std::string& name) const;
};
