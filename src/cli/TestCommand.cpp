#include "cli/TestCommand.h"
#include "cli/ArgParser.h"
#include "cli/RunCommand.h"

#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

void TestCommand::buildArgs(ArgParser& parser) const {
    parser.addPositional("filter", "Optional test filter (substring match)", false);
    parser.addFlag("list", 'l', "List available test files");
    parser.addFlag("quiet", 'q', "Suppress output");
}

int TestCommand::run(const ArgParser& parser) {
    // Find tests/main.lc relative to the project
    // Try common locations
    std::string testPath;
    for (auto& candidate : {"tests/main.lc", "../tests/main.lc", "../../tests/main.lc"}) {
        if (fs::exists(candidate)) {
            testPath = fs::canonical(candidate).string();
            break;
        }
    }

    if (testPath.empty()) {
        // Search up from cwd
        fs::path cur = fs::current_path();
        while (true) {
            auto candidate = cur / "tests" / "main.lc";
            if (fs::exists(candidate)) {
                testPath = candidate.string();
                break;
            }
            if (!cur.has_parent_path() || cur == cur.parent_path()) break;
            cur = cur.parent_path();
        }
    }

    if (testPath.empty()) {
        std::cerr << "lucis: could not find tests/main.lc\n";
        return 1;
    }

    // Reuse RunCommand logic to execute the test suite
    RunCommand runCmd;
    ArgParser runParser("lucis run", "Compile and execute a Lucis program via JIT");
    runCmd.buildArgs(runParser);

    // Build fake argv: [prog, file, -q?]
    std::vector<const char*> fakeArgv;
    fakeArgv.push_back("lucis");
    fakeArgv.push_back(testPath.c_str());
    if (parser.has("quiet"))
        fakeArgv.push_back("-q");

    if (!runParser.parse(fakeArgv.size(), const_cast<char**>(fakeArgv.data())))
        return 1;

    return runCmd.run(runParser);
}
