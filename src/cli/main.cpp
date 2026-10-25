#include "../lexer/tokenizer.h"
#include "../parser/parser.h"
#include "../sema/sema.h"
#include "../codegen/c_emitter.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <sys/wait.h>
#include <utility>

#ifndef SELENIUM_RUNTIME_C
#error "SELENIUM_RUNTIME_C must be defined"
#endif

namespace {

std::string shell_quote(const std::string &value) {
    std::string quoted = "'";
    for (char c : value) {
        if (c == '\'') quoted += "'\\''";
        else quoted += c;
    }
    quoted += "'";
    return quoted;
}

std::filesystem::path create_temp_directory() {
    const auto base = std::filesystem::temp_directory_path();
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::random_device random;
    for (int attempt = 0; attempt < 16; ++attempt) {
        const auto candidate = base / ("selenium-" + std::to_string(ticks) + "-" +
                                       std::to_string(random()));
        std::error_code error;
        if (std::filesystem::create_directory(candidate, error)) return candidate;
        if (error && error != std::errc::file_exists) {
            throw std::runtime_error("cannot create temporary directory: " + error.message());
        }
    }
    throw std::runtime_error("cannot create a unique temporary directory for --run");
}

struct TemporaryDirectory {
    explicit TemporaryDirectory(std::filesystem::path value) : path(std::move(value)) {}
    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    std::filesystem::path path;
};

int shell_status(int status) {
    if (status == -1) return 1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: seleniumc <file.sel> [-o <output.c> | --run] [--cc <compiler>]\n";
        return 1;
    }
    if (std::string(argv[1]) == "--support") {
        std::cout << "Support Selenium development:\n";
        std::cout << "  Ko-fi: https://ko-fi.com/ewancroft\n";
        std::cout << "  GitHub Sponsors: https://github.com/sponsors/ewanc26\n";
        return 0;
    }

    const std::string input = argv[1];
    std::string output;
    std::string cc = "gcc";
    bool run = false;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "seleniumc: " << arg << " requires a path\n";
                return 1;
            }
            output = argv[++i];
        } else if (arg == "--run") {
            run = true;
        } else if (arg == "--cc") {
            if (i + 1 >= argc) {
                std::cerr << "seleniumc: --cc requires a compiler executable path\n";
                return 1;
            }
            cc = argv[++i];
        } else {
            std::cerr << "seleniumc: unknown option " << arg << "\n";
            return 1;
        }
    }
    if (run && !output.empty()) {
        std::cerr << "seleniumc: choose either --run or -o/--output, not both\n";
        return 1;
    }
    if (!run && output.empty()) {
        std::cerr << "seleniumc: specify -o <output.c> or --run\n";
        return 1;
    }

    std::ifstream in(input);
    if (!in) {
        std::cerr << "seleniumc: cannot read source file: " << input << "\n";
        return 1;
    }
    std::stringstream source_stream;
    source_stream << in.rdbuf();
    const std::string source = source_stream.str();

    try {
        Tokenizer tokenizer(source);
        const auto tokens = tokenizer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        SemanticAnalyzer sema;
        sema.analyze(program);
        std::string ccode = emitProgram(program);

        if (!run) {
            std::ofstream out(output);
            if (!out) {
                std::cerr << "seleniumc: cannot create output file: " << output << "\n";
                return 1;
            }
            out << ccode << "\n#include \"" << SELENIUM_RUNTIME_C << "\"\n";
            if (!out) {
                std::cerr << "seleniumc: failed while writing output file: " << output << "\n";
                return 1;
            }
            return 0;
        }

        TemporaryDirectory temp(create_temp_directory());
        const auto cfile = (temp.path / "selenium_out.c").string();
        const auto exefile = (temp.path / "selenium_out").string();
        {
            std::ofstream generated(cfile);
            if (!generated) {
                std::cerr << "seleniumc: cannot create temporary C source\n";
                return 1;
            }
            generated << ccode << "\n#include \"" << SELENIUM_RUNTIME_C << "\"\n";
            if (!generated) {
                std::cerr << "seleniumc: failed while writing temporary C source\n";
                return 1;
            }
        }

        const std::string compile_command = shell_quote(cc) + " " + shell_quote(cfile) +
                                            " -o " + shell_quote(exefile) + " -lm";
        const int compile_status = std::system(compile_command.c_str());
        if (compile_status == -1) {
            std::cerr << "seleniumc: failed to start host C compiler\n";
            return 1;
        }
        if (shell_status(compile_status) != 0) return shell_status(compile_status);

        const int run_status = std::system(shell_quote(exefile).c_str());
        if (run_status == -1) {
            std::cerr << "seleniumc: failed to start generated program\n";
            return 1;
        }
        return shell_status(run_status);
    } catch (const std::exception &e) {
        std::cerr << "seleniumc: " << e.what() << "\n";
        return 1;
    }
}
