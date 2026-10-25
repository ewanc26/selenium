#include "../lexer/tokenizer.h"
#include "../parser/parser.h"
#include "../sema/sema.h"
#include "../codegen/c_emitter.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>

#ifndef SELENIUM_RUNTIME_C
#error "SELENIUM_RUNTIME_C must be defined"
#endif

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: seleniumc <file.sel> [-o output] [--run] [--cc gcc]\n";
        return 1;
    }
    if (std::string(argv[1]) == "--support") {
        std::cout << "Support Selenium development:\n";
        std::cout << "  Ko-fi: https://ko-fi.com/ewancroft\n";
        std::cout << "  GitHub Sponsors: https://github.com/sponsors/ewanc26\n";
        return 0;
    }
    std::string input = argv[1];
    std::string output;
    bool run = false;
    std::string cc = "gcc";
    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-o" && i+1 < argc) output = argv[++i];
        else if (a == "--run") run = true;
        else if (a == "--cc" && i+1 < argc) cc = argv[++i];
    }
    if (!run && output.empty()) { std::cerr << "Either -o/--output or --run must be specified\n"; return 1; }
    
    std::ifstream in(input);
    if (!in) { std::cerr << "cannot open file\n"; return 1; }
    std::stringstream ss; ss << in.rdbuf();
    std::string source = ss.str();
    
    try {
        Tokenizer t(source);
        auto tokens = t.tokenize();
        Parser p(tokens);
        auto prog = p.parse();
        SemanticAnalyzer sema;
        sema.analyze(prog);
        std::string ccode = emitProgram(prog);
        
        if (run) {
            // write temp files
            std::filesystem::path tmpdir = std::filesystem::temp_directory_path() / "selenium_XXXXXX";
            std::string tmpdirstr = tmpdir.string();
            // simple approach
            std::string cfile = "/tmp/selenium_out.c";
            std::string exefile = "/tmp/selenium_out";
            std::ofstream out(cfile);
            out << ccode;
            out << "\n#include \"" << SELENIUM_RUNTIME_C << "\"\n";
            out.close();
            std::string cmd = cc + " " + cfile + " -o " + exefile + " -lm 2>&1";
            int r = system(cmd.c_str());
            if (r != 0) return 1;
            r = system(exefile.c_str());
            return WEXITSTATUS(r);
        } else {
            std::ofstream out(output);
            out << ccode;
            out << "\n#include \"" << SELENIUM_RUNTIME_C << "\"\n";
        }
    } catch (std::exception &e) {
        std::cerr << "seleniumc: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
