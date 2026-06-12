#pragma once

#include <string>
#include <vector>

class ArgParser;

class Command {
public:
    virtual ~Command() = default;

    // Name used in `lucis <name>` and `lucis help <name>`
    virtual std::string name() const = 0;

    // Short description for help listing
    virtual std::string description() const = 0;

    // Build the argument parser for this command
    virtual void buildArgs(ArgParser& parser) const = 0;

    // Execute the command. Returns exit code.
    virtual int run(const ArgParser& parser) = 0;
};
