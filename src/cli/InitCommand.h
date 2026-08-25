#pragma once

#include "cli/Command.h"
#include "config/LucisConfig.h"
#include "ArgParser.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

class InitCommand : public Command {
public:
    std::string name() const override { return "init"; }
    std::string description() const override {
        return "Create a new Lucis project with lucis.yaml";
    }
    void buildArgs(ArgParser& parser) const override {
        parser.addPositional("path", "Project directory or name (default: .)");
    }
    int run(const ArgParser& parser) override {
        std::string path = parser.get("path");
        if (path.empty()) path = ".";

        fs::path target(path);
        bool exists = fs::exists(target);

        if (!exists) {
            std::error_code ec;
            fs::create_directories(target, ec);
            if (ec) {
                std::cerr << "error: could not create directory '" << path
                          << "': " << ec.message() << "\n";
                return 1;
            }
            std::cout << "created directory '" << path << "/'\n";
        }

        {
            std::error_code ec;
            fs::create_directories(target / "src", ec);
        }

        auto absPath = fs::absolute(target).string();
        auto dirName = target.filename().string();
        if (dirName.empty() || dirName == ".") {
            dirName = fs::absolute(fs::current_path()).filename().string();
        }

        auto mainPath = target / "src" / "main.lc";
        if (!fs::exists(mainPath)) {
            std::ofstream ofs(mainPath);
            ofs << "use stdio::printf;\n\n"
                << "fn main() int32 {\n"
                << "    printf(\"Hello, {s}\\n\", \"Lucis!\");\n"
                << "    return 0;\n"
                << "}\n";
            std::cout << "created file '" << (fs::relative(mainPath, absPath)).string() << "'\n";
        }

        if (!LucisConfig::createDefault(absPath, dirName)) {
            std::cerr << "error: could not create lucis.yaml\n";
            return 1;
        }

        // Generated .gitignore: build artifacts and pipeline caches should
        // never be committed. Existing files are left untouched.
        auto gitignorePath = target / ".gitignore";
        if (!fs::exists(gitignorePath)) {
            std::ofstream ofs(gitignorePath);
            ofs << "# Lucis — build artifacts and pipeline caches\n"
                << "build/\n"
                << "dist/\n"
                << "*.o\n"
                << "*.out\n"
                << "*.log\n"
                << ".lucis/\n"
                << ".lucisbuild/\n"
                << ".luciscache/\n"
                << "\n"
                << "# lucis fuzz artifacts (generated inputs, crashes, seeds)\n"
                << "fuzz/crashes/\n"
                << "fuzz/corpus/id_*\n";
            std::cout << "created file '.gitignore'\n";
        }

        std::cout << "created file 'lucis.yaml'\n";
        std::cout << "done. Run `lucis build` or `lucis run` from this directory to compile.\n";
        return 0;
    }
};
