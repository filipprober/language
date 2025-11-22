#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <memory>

#include "sourcelocation.h"
#include "exceptionreporter.h"
#include "lexer/Token.h"
#include "lexer/Lexer.h"
#include "tokenTypeToString.h"
#include "parser/parser.h"
#include "ast/AstNode.h"
#include "typechecker.h"
#include "codegenerator/codegenerator.h"

using namespace std;

ostream& operator<<(ostream& os, const Token& token);

ostream& operator<<(ostream& os, const Token& token) {
    os  << "Token(" << tokenTypeToString(token.type)
        << ", \"" << token.lexeme << "\", "
        << token.location.line << ":" << token.location.column << ")";

    return os;
}

void showUsage() {
    cerr << "USAGE:" << endl;
    cerr << string(2, ' ') << "mylang <file.ml> [-o output]" << endl;
    cerr << "OPTIONS:" << endl;
    cerr << string(2, ' ') << "--debug" << endl;
    cerr << string(2, ' ') << "--no-run" << endl;
}

/**
 * Show all extracted tokens.
 */
void show(const vector<Token>& tokens) {
    cout << "=== TOKENS ===" << endl;
    for (const Token& token : tokens) {
        cout << token << endl;
    }
    cout << endl;
}

/**
 * Show generated Abstract Syntax Tree.
 */
void show(const unique_ptr<Program>& program) {
    cout << "=== AST ===" << endl;
    program->print();
    cout << endl;
}

/**
 * Show generated LLVM code.
 */
void show(CodeGenerator& codegen) {
    cout << "=== LLVM IR ===" << endl;
    codegen.printIR();
    cout << endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        showUsage();
        return 1;
    }

    string inputFile = argv[1];
    string outputFile = "output.o";
    string executableFile = "a.out";
    bool debug = false;
    bool run = true;

    for (int i = 2; i < argc; i++) {
        if (string(argv[i]) == "-o" && i + 1 < argc) {
            executableFile = argv[i + 1];
            i++;
        } else if (string(argv[i]) == "--debug") {
            debug = true;
        } else if (string(argv[i]) == "--no-run") {
            run = false;
        }
    }

    ifstream file(inputFile);
    if (!file) {
        cerr << "Could not open file: " << inputFile << endl;
        return 1;
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string source = buffer.str();

    ExceptionReporter reporter(source, inputFile);

    try {
        Lexer lexer(source, reporter);
        auto tokens = lexer.tokenize();

        if (reporter.hasError()) {
            return 1;
        }

        // Show all extracted tokens.
        if (debug) {
            show(tokens);
        }

        Parser parser(tokens, reporter);
        auto program = parser.parse();

        if (reporter.hasError()) {
            return 1;
        }

        // Show generated Abstract Syntax Tree.
        if (debug) {
            show(program);
        }

        TypeChecker typeChecker(reporter);
        typeChecker.check(program.get());

        if (reporter.hasError()) {
            return 1;
        }

        CodeGenerator codegen;
        codegen.generate(*program);

        // Show generated LLVM code.
        if (debug) {
            show(codegen);
        }

        codegen.writeObjectFile(outputFile);

        if (run) {
            string linkCommand = "clang " + outputFile + " -o " + executableFile + " 2>/dev/null";
            int linkResult = system(linkCommand.c_str());

            if (linkResult != 0) {
                cerr << "Linking failed!" << endl;
                return 1;
            }

            string execCommand = "./" + executableFile;
            int execResult = system(execCommand.c_str());

            remove(outputFile.c_str());
            remove(executableFile.c_str());

            return WEXITSTATUS(execResult);
        }

    } catch (const exception& e) {
        if (!reporter.hasError()) {
            cerr << "Internal error: " << e.what() << endl;
        }
        return 1;
    }

    return 0;
}