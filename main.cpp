#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <memory>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"

using namespace std;
using namespace llvm;

struct SourceLocation
{
    int line;
    int column;
    int length;

    SourceLocation(int line, int column, int length) :
        line(line),
        column(column),
        length(length) {
        //
    }
};

class ExceptionReporter
{
public:
    ExceptionReporter(const string& source, const string& filename) :
        source(source),
        filename(filename),
        hasErrors(false) {
        //
    }

    void error(const SourceLocation& location, const string& message) {
        hasErrors = true;

        cerr << "\033[1;31mError:\033[0m " << message << endl;
        cerr << "  \033[1;34m-->\033[0m " << filename << ":" << location.line << ":" << location.column << endl;

        printSourceLine(location);
    }

    void warning(const SourceLocation& location, const string& message) {
        cerr << "\033[1;33mWarning:\033[0m " << message << endl;
        cerr << "  \033[1;34m-->\033[0m " << filename << ":" << location.line << ":" << location.column << endl;

        printSourceLine(location);
    }

    bool hasError() const {
        return hasErrors;
    }

private:
    string source;
    string filename;
    bool hasErrors;

    void printSourceLine(const SourceLocation& location) {
        // Split source into lines
        vector<string> lines;
        stringstream ss(source);
        string line;
        while (getline(ss, line)) {
            lines.push_back(line);
        }

        if (location.line < 1 || location.line > static_cast<int>(lines.size())) {
            return;
        }

        string sourceLine = lines[location.line - 1];

        // Line number padding
        int lineNumWidth = to_string(location.line).length();
        string padding(lineNumWidth, ' ');

        cerr << padding << " |" << endl;
        cerr << location.line << " | " << sourceLine << endl;
        cerr << padding << " | ";

        // Print spaces until error position
        for (int i = 0; i < location.column - 1; i++) {
            cerr << " ";
        }

        // Print error indicator (^^^)
        cerr << "\033[1;31m";
        for (int i = 0; i < location.length; i++) {
            cerr << "^";
        }
        cerr << "\033[0m" << endl;
        cerr << endl;
    }
};

enum class TokenType
{
    // ======================
    // Keywords
    // ======================

    NAMESPACE, USE,
    CLASS, ABSTRACT, INTERFACE, TRAIT, ENUM,
    EXTENDS, IMPLEMENTS,
    PUBLIC, PRIVATE, PROTECTED,
    FN, VAR, CONST, INIT, THIS, SUPER, END,
    IF, ELSE, WHILE, FOR, MATCH, CASE, DEFAULT,
    RETURN, BREAK, CONTINUE, WHEN,
    ASYNC, AWAIT,
    TRY, CATCH, FINALLY, THROW,

    // ======================
    // Types
    // ======================

    INT, FLOAT, STRING, BOOL, VOID, ANY,

    // ======================
    // Literals
    // ======================

    TRUE, FALSE, NONE,
    IDENTIFIER,
    INTEGER_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,

    // ======================
    // Operators
    // ======================

    PLUS, MINUS, STAR, SLASH, PERCENT,
    EQUAL, EQUAL_EQUAL, BANG_EQUAL,
    LESS, LESS_EQUAL, GREATER, GREATER_EQUAL,
    PLUS_EQUAL, MINUS_EQUAL, STAR_EQUAL, SLASH_EQUAL,

    // ======================
    // Logical
    // ======================

    AND, OR, NOT, IS, AS, IN,

    // ======================
    // Delimeters
    // ======================

    LEFT_PAREN, RIGHT_PAREN,
    LEFT_BRACE, RIGHT_BRACE,
    LEFT_BRACKET, RIGHT_BRACKET,
    COMMA, DOT, COLON, SEMICOLON,
    ARROW, FAT_ARROW, DOUBLE_COLON,

    // ======================
    // Special
    // ======================

    NEWLINE, INDENT, DEDENT,
    EOF_TOKEN, UNKNOWN,
};

struct Token
{
    TokenType type;
    string lexeme;
    SourceLocation location;

    Token(TokenType type, const string& lexeme, int line, int column) :
        type(type),
        lexeme(lexeme),
        location(line, column, lexeme.length())
    {
        //
    }

    Token(TokenType type, const string& lexeme, SourceLocation location) :
        type(type),
        lexeme(lexeme),
        location(location)
    {

    }
};

string tokenTypeToString(TokenType type);
ostream& operator<<(ostream& os, const Token& token);

class ASTNode;
class Expression;
class Statement;

// ======================
// AST BASE CLASSES
// ======================
class ASTNode
{
public:
    virtual ~ASTNode() {}
    virtual void print(int indent = 0) const = 0;
};

class Expression : public ASTNode {
public:
    virtual ~Expression() override = default;
};

class Statement : public ASTNode {
public:
    virtual ~Statement() override = default;
};

// ======================
// Expressions
// ======================

class IntLiteral : public Expression {
public:
    int value;

    explicit IntLiteral(int value) : value(value) {
        //
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "IntLiteral(" << value << ")" << endl;
    }
};

class StringLiteral : public Expression {
public:
    string value;

    explicit StringLiteral(string value) : value(value) {
        //
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "StringLiteral(" << value << ")" << endl;
    }
};

class BoolLiteral : public Expression {
public:
    bool value;

    explicit BoolLiteral(bool value) : value(value) {
        //
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "BoolLiteral(" << (value ? "true" : "false") << ")" << endl;
    }
};

class Variable : public Expression {
public:
    string name;

    explicit Variable(const string& name) : name(name) {
        //
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "Variable(" << name << ")" << endl;
    }
};

class BinaryOperation : public Expression {
public:
    unique_ptr<Expression> left;
    TokenType op;
    unique_ptr<Expression> right;

    BinaryOperation(unique_ptr<Expression> left, TokenType op, unique_ptr<Expression> right) :
        left(std::move(left)),
        op(op),
        right(std::move(right)) {
        //
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "BinaryOperation(" << tokenTypeToString(op) << ")" << endl;
        left->print(indent + 2);
        right->print(indent + 2);
    }
};

class Assignment : public Expression
{
public:
    string name;
    unique_ptr<Expression> value;

    Assignment(const string& name, unique_ptr<Expression> value) :
        name(name),
        value(std::move(value)) {
        //
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "Assignment(" << name << ")" << endl;
        value->print(indent + 2);
    }
};

class UnaryOperation : public Expression {
public:
    TokenType op;
    unique_ptr<Expression> operand;

    UnaryOperation(TokenType op, unique_ptr<Expression> operand) :
        op(op),
        operand(std::move(operand)) {
        //
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "UnaryOperation(" << tokenTypeToString(op) << ")" << endl;
        operand->print(indent + 2);
    }
};

// ======================
// Statements
// ======================

class VariableDeclaration : public Statement {
public:
    string name;
    string type; // Optional
    unique_ptr<Expression> initializer;

    VariableDeclaration(const string& name, const string& type, unique_ptr<Expression> initializer) :
        name(name),
        type(type),
        initializer(std::move(initializer)) {
        //
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "VariableDeclaration(name=" << name;
        if (!type.empty()) {
            cout << ", type=" << type;
        }
        cout << ")" << endl;
        if (initializer) {
            initializer->print(indent + 2);
        }
    }
};

class ReturnStatement : public Statement {
public:
    unique_ptr<Expression> value;

    explicit ReturnStatement(unique_ptr<Expression> value) :
        value(std::move(value)) {}

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "ReturnStatement" << endl;
        if (value) {
            value->print(indent + 2);
        }
    }
};

class ExpressionStatement : public Statement {
public:
    unique_ptr<Expression> expression;

    explicit ExpressionStatement(unique_ptr<Expression> expression) :
        expression(std::move(expression)) {}

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "ExpressionStatement" << endl;
        expression->print(indent + 2);
    }
};

// ======================
// Function Related Nodes
// ======================

// Function Parameter: (name: type)
class Parameter {
public:
    string name;
    string type;

    Parameter(const string& name, const string& type) :
        name(name),
        type(type) {
        //
    }
};

class FunctionDeclaration : public Statement {
public:
    string name;
    vector<Parameter> parameters;
    string returnType;
    vector<unique_ptr<Statement>> body;

    FunctionDeclaration(const string& name, vector<Parameter> parameters, const string& returnType, vector<unique_ptr<Statement>> body) :
        name(name),
        parameters(std::move(parameters)),
        returnType(returnType),
        body(std::move(body)) {
        //
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "FunctionDeclaration(name=" << name;

        cout << ", params=[";
        for (size_t i = 0; i < parameters.size(); i++) {
            if (i > 0) cout << ", ";
            cout << parameters[i].name << ": " << parameters[i].type;
        }
        cout << "]";

        if (!returnType.empty()) {
            cout << ", returns=" << returnType;
        }
        cout << ")" << endl;

        for (const auto& statement : body) {
            statement->print(indent + 2);
        }
    }
};

// Function Call: add(5, 3);
class FunctionCall : public Expression {
public:
    string name;
    vector<unique_ptr<Expression>> arguments;

    FunctionCall(const string& name, vector<unique_ptr<Expression>> arguments) :
        name(name),
        arguments(std::move(arguments)) {
        //
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "FunctionCall(" << name << ")" << endl;
        for (const auto& argument : arguments) {
            argument->print(indent + 2);
        }
    }
};

// If Statement: if x > 5:
class IfStatement : public Statement {
public:
    unique_ptr<Expression> condition;
    vector<unique_ptr<Statement>> thenBranch;
    vector<unique_ptr<Statement>> elseBranch; // Optional

    IfStatement(
        unique_ptr<Expression> condition,
        vector<unique_ptr<Statement> > thenBranch,
        vector<unique_ptr<Statement> > elseBranch
    ) : condition(std::move(condition)),
        thenBranch(std::move(thenBranch)),
        elseBranch(std::move(elseBranch)) {
        //
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "IfStatement" << endl;
        cout << string(indent + 2, ' ') << "Condition:" << endl;
        condition->print(indent + 4);
        cout << string(indent + 2, ' ') << "Then:" << endl;

        for (const auto& statement: thenBranch) {
            statement->print(indent + 4);
        }

        if (!elseBranch.empty()) {
            cout << string(indent + 2, ' ') << "Else:" << endl;
            for (const auto& statement: elseBranch) {
                statement->print(indent + 4);
            }
        }
    }
};

// While Statement: while x < 10:
class WhileStatement : public Statement {
public:
    unique_ptr<Expression> condition;
    vector<unique_ptr<Statement>> body;

    WhileStatement(
        unique_ptr<Expression> condition,
        vector<unique_ptr<Statement>> body
    ) : condition(std::move(condition)),
        body(std::move(body)) {}

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "WhileStatement" << endl;
        cout << string(indent + 2, ' ') << "Condition:" << endl;
        condition->print(indent + 4);
        cout << string(indent + 2, ' ') << "Body:" << endl;

        for (const auto& statement: body) {
            statement->print(indent + 4);
        }
    }
};

// ======================
// Return Path Analysis
// ======================

class ReturnPathAnalyzer
{
public:
    // Prüft, ob alle Code-Pfade einen Return haben
    static bool hasReturnOnAllPaths(const vector<unique_ptr<Statement>>& statements) {
        for (size_t i = 0; i < statements.size(); i++) {
            const auto& stmt = statements[i];

            if (dynamic_cast<ReturnStatement*>(stmt.get())) {
                return true;
            }

            // If-Statement mit else
            if (auto* ifStmt = dynamic_cast<IfStatement*>(stmt.get())) {
                // Nur wenn BEIDE Zweige existieren UND beide returnen
                if (!ifStmt->elseBranch.empty()) {
                    bool thenReturns = hasReturnOnAllPaths(ifStmt->thenBranch);
                    bool elseReturns = hasReturnOnAllPaths(ifStmt->elseBranch);

                    if (thenReturns && elseReturns) {
                        // Beide Zweige returnen -> diese if-else Statement gilt als "returning"
                        // Aber wir müssen weitermachen, falls danach noch Code kommt
                        // (sollte eigentlich "unreachable code" sein, aber egal)
                        return true;
                    }
                }
                // Wenn if kein else hat oder nicht beide returnen, weitermachen
            }
            // While-Schleifen garantieren keinen Return (können 0 mal laufen)
            // Also ignorieren wir sie hier
        }

        return false;
    }

    // Prüft eine Funktion auf korrekte Returns
    static void validateFunction(const FunctionDeclaration* funcDecl) {
        string funcName = funcDecl->name;
        string returnType = funcDecl->returnType;

        // main() wird speziell behandelt
        if (funcName == "main") {
            return; // main darf implizit 0 zurückgeben
        }

        // void-Funktionen brauchen keinen expliziten Return
        if (returnType == "void" || returnType.empty()) {
            return;
        }

        if (!hasReturnOnAllPaths(funcDecl->body)) {
            throw runtime_error(
                "Function '" + funcName + "' with return type '" + returnType +
                "' does not return a value on all code paths"
            );
        }
    }
};

// ======================
// Program (Root Node)
// ======================

class Program : public ASTNode {
public:
    vector<unique_ptr<Statement>> statements;

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "Program" << endl;

        for (const unique_ptr<Statement>& statement : statements) {
            statement->print(indent + 2);
        }
    }
};

// ======================
// Parser
// ======================

class Parser {
public:
    explicit Parser(vector<Token> tokens, ExceptionReporter& reporter) :
        tokens(std::move(tokens)),
        current(0),
        reporter(reporter)
    {
        //
    }

    unique_ptr<Program> parse() {
        auto program = make_unique<Program>();

        while (!isAtEnd()) {
            // Skip newlines and DEDENT tokens at top level
            while (match({TokenType::NEWLINE})) {}
            while (match({TokenType::DEDENT})) {}

            if (isAtEnd()) break;

            try {
                auto statement = parseStatement();
                if (statement) {
                    program->statements.push_back(std::move(statement));
                }
            } catch (const runtime_error& e) {
                synchronize();
            }
        }

        return program;
    }

private:
    vector<Token> tokens;
    size_t current;
    ExceptionReporter& reporter;

    bool isAtEnd() const {
        return peek().type == TokenType::EOF_TOKEN;
    }

    Token peek() const {
        return tokens[current];
    }

    Token previous() const {
        return tokens[current - 1];
    }

    Token advance() {
        if (!isAtEnd()) current++;
        return previous();
    }

    bool check(TokenType type) const {
        if (isAtEnd()) return false;
        return peek().type == type;
    }

    bool match(TokenType type) {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    bool match(initializer_list<TokenType> types) {
        for (TokenType type : types) {
            if (check(type)) {
                advance();
                return true;
            }
        }
        return false;
    }

    Token consume(TokenType type, const string& message) {
        if (check(type)) return advance();

        // Finde das letzte nicht-whitespace Token für bessere Fehlerposition
        Token errorToken = previous();

        // Skip zurück über NEWLINE tokens
        size_t pos = current - 1;
        while (pos > 0 && tokens[pos].type == TokenType::NEWLINE) {
            pos--;
        }
        if (pos > 0) {
            errorToken = tokens[pos];
        }

        // Erstelle informative Fehlermeldung
        string fullMessage = message;
        Token nextToken = peek();

        if (nextToken.type != TokenType::EOF_TOKEN) {
            fullMessage += ", got '" + nextToken.lexeme + "'";
        }

        reporter.error(errorToken.location, fullMessage);
        throw runtime_error("Parse error");
    }

    void reportError(const string& message) {
        reporter.error(peek().location, message);
        throw runtime_error("Parse exception");
    }

    void synchronize() {
        advance();

        while (!isAtEnd()) {
            if (previous().type == TokenType::NEWLINE) return;

            switch (peek().type) {
                case TokenType::CLASS:
                case TokenType::FN:
                case TokenType::VAR:
                case TokenType::IF:
                case TokenType::WHILE:
                case TokenType::RETURN:
                    break;
                default:
                    break;
            }

            advance();
        }
    }

    // Statement parsing

    unique_ptr<Statement> parseStatement() {
        // Function: fn add(a: int, b: int) -> int:
        if (match(TokenType::FN)) {
            return parseFunctionDeclaration();
        }

        // Variable: var x = 5
        if (match(TokenType::VAR)) {
            return parseVarDeclaration();
        }

        // If: if x > 5:
        if (match(TokenType::IF)) {
            return parseIfStatement();
        }

        // While: while x < 10:
        if (match(TokenType::WHILE)) {
            return parseWhileStatement();
        }

        // Return: return 42
        if (match(TokenType::RETURN)) {
            return parseReturnStatement();
        }

        // Expression statement
        return parseExpressionStatement();
    }

    // Parse Function Declaration
    unique_ptr<Statement> parseFunctionDeclaration() {
        Token name = consume(TokenType::IDENTIFIER, "Expected function name");

        consume(TokenType::LEFT_PAREN, "Expected '(' after function name");

        // Parse parameters
        vector<Parameter> parameters;
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                Token paramName = consume(TokenType::IDENTIFIER, "Expected parameter name");
                consume(TokenType::COLON, "Expected ':' after parameter name");

                if (!match({TokenType::INT, TokenType::FLOAT, TokenType::STRING, TokenType::BOOL, TokenType::VOID})) {
                    reportError("Expected type after ':'");
                }
                Token paramType = previous();
                parameters.emplace_back(paramName.lexeme, paramType.lexeme);
            } while (match(TokenType::COMMA));
        }

        consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters");

        // Parse return type (optional)
        string returnType = "void";
        if (match(TokenType::ARROW)) {
            if (!match({TokenType::INT, TokenType::FLOAT, TokenType::STRING, TokenType::BOOL, TokenType::VOID})) {
                throw runtime_error("Expected return type after '->'");
            }
            returnType = previous().lexeme;
        }

        consume(TokenType::COLON, "Expected ':' after function signature");

        // Skip all newlines
        while (match(TokenType::NEWLINE)) {}

        // Parse body (indented block)
        vector<unique_ptr<Statement>> body = parseBlock();

        // Erstelle die FunctionDeclaration
        auto funcDecl = make_unique<FunctionDeclaration>(
            name.lexeme,
            std::move(parameters),
            returnType,
            std::move(body)
        );

        // Validiere Return-Pfade
        ReturnPathAnalyzer::validateFunction(funcDecl.get());

        return funcDecl;
    }

    // Parse If Statement
    unique_ptr<Statement> parseIfStatement() {
        auto condition = parseExpression();
        consume(TokenType::COLON, "Expected ':' after if condition");

        // Skip all newlines
        while (match(TokenType::NEWLINE)) {}

        vector<unique_ptr<Statement> > thenBranch = parseBlock();
        vector<unique_ptr<Statement> > elseBranch;

        if (match(TokenType::ELSE)) {
            consume(TokenType::COLON, "Expected ':' after else");
            match(TokenType::NEWLINE);
            elseBranch = parseBlock();
        }

        return make_unique<IfStatement>(
            std::move(condition),
            std::move(thenBranch),
            std::move(elseBranch)
        );
    }

    // Parse While Statement
    unique_ptr<Statement> parseWhileStatement() {
        auto condition = parseExpression();
        consume(TokenType::COLON, "Expected ':' after while condition");

        // Skip all newlines
        while (match(TokenType::NEWLINE)) {}

        vector<unique_ptr<Statement> > body = parseBlock();

        return make_unique<WhileStatement>(
            std::move(condition),
            std::move(body)
        );
    }

    vector<unique_ptr<Statement>> parseBlock() {
        vector<unique_ptr<Statement>> statements;

        // Expect INDENT at start of block
        consume(TokenType::INDENT, "Expected indentation after ':'");

        // Skip initial newlines
        while (match(TokenType::NEWLINE)) {}

        // Parse statements until DEDENT
        while (!isAtEnd()) {
            // Check for DEDENT - das ist das Ende des Blocks
            if (check(TokenType::DEDENT)) {
                advance(); // consume the DEDENT
                break;
            }

            // Skip empty lines
            while (match(TokenType::NEWLINE)) {}

            // Check again after skipping newlines
            if (check(TokenType::DEDENT)) {
                advance();
                break;
            }

            if (isAtEnd()) {
                break;
            }

            // Parse the statement
            statements.push_back(parseStatement());
        }

        return statements;
    }

    unique_ptr<Statement> parseVarDeclaration() {
        Token name = consume(TokenType::IDENTIFIER, "Expected variable name");

        string type;

        // Optional type annotation
        if (match(TokenType::COLON)) {
            Token typeToken = advance();
            type = typeToken.lexeme;
        }

        unique_ptr<Expression> initializer = nullptr;

        if (match(TokenType::EQUAL)) {
            initializer = parseExpression();
        }

        match(TokenType::NEWLINE);

        return make_unique<VariableDeclaration>(name.lexeme, type, std::move(initializer));
    }

    unique_ptr<Statement> parseReturnStatement() {
        unique_ptr<Expression> value = nullptr;

        if (!check(TokenType::NEWLINE) && !isAtEnd()) {
            value = parseExpression();
        }

        match(TokenType::NEWLINE);

        return make_unique<ReturnStatement>(std::move(value));
    }

    unique_ptr<Statement> parseExpressionStatement() {
        auto expression = parseExpression();

        match(TokenType::NEWLINE);

        return make_unique<ExpressionStatement>(std::move(expression));
    }

    // ==================
    // Expression Parsing
    // ==================
    unique_ptr<Expression> parseExpression() {
        return parseAssignment();
    }

    unique_ptr<Expression> parseAssignment() {
        auto expr = parseLogicalOr();

        // Check for assignment
        if (match(TokenType::EQUAL)) {
            // Left side must be a variable
            auto* var = dynamic_cast<Variable*>(expr.get());
            if (!var) {
                reportError("Invalid assignment target");
            }

            string name = var->name;
            auto value = parseAssignment(); // Right associative
            return make_unique<Assignment>(name, std::move(value));
        }

        return expr;
    }

    unique_ptr<Expression> parseLogicalOr() {
        auto expr = parseLogicalAnd();

        while (match(TokenType::OR)) {
            TokenType op = previous().type;
            auto right = parseLogicalAnd();
            expr = make_unique<BinaryOperation>(std::move(expr), op, std::move(right));
        }

        return expr;
    }

    unique_ptr<Expression> parseLogicalAnd() {
        auto expr = parseComparison();

        while (match(TokenType::AND)) {
            TokenType op = previous().type;
            auto right = parseComparison();
            expr = make_unique<BinaryOperation>(std::move(expr), op, std::move(right));
        }

        return expr;
    }

    // Comparison: ==, !=, <, >, <=, >=
    unique_ptr<Expression> parseComparison() {
        auto expr = parseAddition();

        while (match({
            TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL,
            TokenType::LESS, TokenType::LESS_EQUAL,
            TokenType::GREATER, TokenType::GREATER_EQUAL
        })) {
            TokenType op = previous().type;
            auto right = parseAddition();
            expr = make_unique<BinaryOperation>(std::move(expr), op, std::move(right));
        }

        return expr;
    }

    unique_ptr<Expression> parseAddition() {
        auto expression = parseStringConcatenation();

        while (match({TokenType::PLUS, TokenType::MINUS})) {
            TokenType op = previous().type;
            auto right = parseStringConcatenation();
            expression = make_unique<BinaryOperation>(std::move(expression), op, std::move(right));
        }

        return expression;
    }

    unique_ptr<Expression> parseStringConcatenation() {
        auto expression = parseMultiplication();

        while (match(TokenType::DOT)) {
            TokenType op = previous().type;
            auto right = parseMultiplication();
            expression = make_unique<BinaryOperation>(std::move(expression), op, std::move(right));
        }

        return expression;
    }

    unique_ptr<Expression> parseMultiplication() {
        auto expression = parseUnary();

        while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
            TokenType op = previous().type;
            auto right = parseUnary();
            expression = make_unique<BinaryOperation>(std::move(expression), op, std::move(right));
        }

        return expression;
    }

    unique_ptr<Expression> parseUnary() {
        if (match({TokenType::MINUS, TokenType::NOT})) {
            TokenType op = previous().type;
            auto operand = parseUnary(); // Recursive for multiple unary ops
            return make_unique<UnaryOperation>(op, std::move(operand));
        }

        return parsePrimary();
    }

    unique_ptr<Expression> parsePrimary() {
        // true/false
        if (match(TokenType::TRUE)) {
            return make_unique<BoolLiteral>(true);
        }
        if (match(TokenType::FALSE)) {
            return make_unique<BoolLiteral>(false);
        }

        // Integer: 42
        if (match(TokenType::INTEGER_LITERAL)) {
            int value = stoi(previous().lexeme);
            return make_unique<IntLiteral>(value);
        }

        // String: "text"
        if (match(TokenType::STRING_LITERAL)) {
            return make_unique<::StringLiteral>(previous().lexeme);
        }

        // Variable oder Function Call
        if (match(TokenType::IDENTIFIER)) {
            string name = previous().lexeme;

            // Function Call: add(5, 3)
            if (match(TokenType::LEFT_PAREN)) {
                vector<unique_ptr<Expression>> arguments;

                // Parse arguments
                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        arguments.push_back(parseExpression());
                    } while (match(TokenType::COMMA));
                }

                consume(TokenType::RIGHT_PAREN, "Expected ')' after arguments");
                return make_unique<FunctionCall>(name, std::move(arguments));
            }

            // Just a variable
            return make_unique<Variable>(name);
        }

        // Grouped: (5 + 3)
        if (match(TokenType::LEFT_PAREN)) {
            auto expression = parseExpression();
            consume(TokenType::RIGHT_PAREN, "Expected ')' after expression");
            return expression;
        }

        reportError("Expected expression");
        return nullptr;
    }
};

string tokenTypeToString(TokenType type) {
    switch (type) {

        // ======================
        // Keywords
        // ======================

        case TokenType::NAMESPACE: return "NAMESPACE";
        case TokenType::USE: return "USE";
        case TokenType::CLASS: return "CLASS";
        case TokenType::ABSTRACT: return "ABSTRACT";
        case TokenType::INTERFACE: return "INTERFACE";
        case TokenType::TRAIT: return "TRAIT";
        case TokenType::ENUM: return "ENUM";
        case TokenType::EXTENDS: return "EXTENDS";
        case TokenType::IMPLEMENTS: return "IMPLEMENTS";
        case TokenType::PUBLIC: return "PUBLIC";
        case TokenType::PROTECTED: return "PROTECTED";
        case TokenType::PRIVATE: return "PRIVATE";
        case TokenType::FN: return "FN";
        case TokenType::VAR: return "VAR";
        case TokenType::CONST: return "CONST";
        case TokenType::INIT: return "INIT";
        case TokenType::THIS: return "THIS";
        case TokenType::SUPER: return "SUPER";
        case TokenType::END: return "END";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::WHILE: return "WHILE";
        case TokenType::FOR: return "FOR";
        case TokenType::MATCH: return "MATCH";
        case TokenType::CASE: return "CASE";
        case TokenType::DEFAULT: return "DEFAULT";
        case TokenType::RETURN: return "RETURN";
        case TokenType::BREAK: return "BREAK";
        case TokenType::CONTINUE: return "CONTINUE";
        case TokenType::WHEN: return "WHEN";
        case TokenType::ASYNC: return "ASYNC";
        case TokenType::AWAIT: return "AWAIT";
        case TokenType::TRY: return "TRY";
        case TokenType::CATCH: return "CATCH";
        case TokenType::FINALLY: return "FINALLY";
        case TokenType::THROW: return "THROW";

        // ======================
        // Types
        // ======================

        case TokenType::INT: return "INT";
        case TokenType::FLOAT: return "FLOAT";
        case TokenType::STRING: return "STRING";
        case TokenType::BOOL: return "BOOL";
        case TokenType::VOID: return "VOID";
        case TokenType::ANY: return "ANY";

        // ======================
        // Literals
        // ======================

        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::NONE: return "NONE";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::INTEGER_LITERAL: return "INTEGER_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";

        // ======================
        // Operators
        // ======================

        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TokenType::BANG_EQUAL: return "BANG_EQUAL";
        case TokenType::LESS: return "LESS";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::GREATER: return "GREATER";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::PLUS_EQUAL: return "PLUS_EQUAL";
        case TokenType::MINUS_EQUAL: return "MINUS_EQUAL";
        case TokenType::STAR_EQUAL: return "STAR_EQUAL";
        case TokenType::SLASH_EQUAL: return "SLASH_EQUAL";

        // ======================
        // Logical
        // ======================

        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::NOT: return "NOT";
        case TokenType::IS: return "IS";
        case TokenType::AS: return "AS";
        case TokenType::IN: return "IN";

        // ======================
        // Delimeters
        // ======================

        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::LEFT_BRACE: return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE: return "RIGHT_BRACE";
        case TokenType::LEFT_BRACKET: return "LEFT_BRACKET";
        case TokenType::RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::DOT: return "DOT";
        case TokenType::COLON: return "COLON";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::ARROW: return "ARROW";
        case TokenType::FAT_ARROW: return "FAT_ARROW";
        case TokenType::DOUBLE_COLON: return "DOUBLE_COLON";

        // ======================
        // Special
        // ======================

        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::INDENT: return "INDENT";
        case TokenType::DEDENT: return "DEDENT";
        case TokenType::EOF_TOKEN: return "EOF_TOKEN";

        default: return "UNKNOWN";
    }
}

ostream& operator<<(ostream& os, const Token& token) {
    os  << "Token(" << tokenTypeToString(token.type)
        << ", \"" << token.lexeme << "\", "
        << token.location.line << ":" << token.location.column << ")";

    return os;
}

class Lexer
{
public:
    explicit Lexer(const string& source, ExceptionReporter& reporter);

    vector<Token> tokenize();

private:
    string source;
    size_t current = 0;
    int line = 1;
    int column = 1;
    vector<Token> tokens;
    ExceptionReporter& reporter;

    vector<int> indentStack = {0};
    bool atLineStart = true;
    int currentIndent = 0;

    static std::unordered_map<string, TokenType> keywords;

    bool isAtEnd() const;
    char peek() const;
    char peekNext() const;
    char advance();

    void addToken(TokenType type, const string& lexeme);
    void scanToken();

    void scanNumber();
    void scanString();
    void scanIdentifier();
    void scanComment();
    void handleIndentation();

    bool isDigit(char c) const;
    bool isAlpha(char c) const;
    bool isAlphaNumeric(char c) const;
};

unordered_map<string, TokenType> Lexer::keywords = {
    {"namespace", TokenType::NAMESPACE},
    {"use", TokenType::USE},
    {"class", TokenType::CLASS},
    {"abstract", TokenType::ABSTRACT},
    {"interface", TokenType::INTERFACE},
    {"trait", TokenType::TRAIT},
    {"enum", TokenType::ENUM},
    {"extends", TokenType::EXTENDS},
    {"implements", TokenType::IMPLEMENTS},
    {"public", TokenType::PUBLIC},
    {"protected", TokenType::PROTECTED},
    {"private", TokenType::PRIVATE},
    {"fn", TokenType::FN},
    {"var", TokenType::VAR},
    {"const", TokenType::CONST},
    {"init", TokenType::INIT},
    {"this", TokenType::THIS},
    {"super", TokenType::SUPER},
    {"end", TokenType::END},
    {"if", TokenType::IF},
    {"else", TokenType::ELSE},
    {"while", TokenType::WHILE},
    {"for", TokenType::FOR},
    {"match", TokenType::MATCH},
    {"case", TokenType::CASE},
    {"default", TokenType::DEFAULT},
    {"return", TokenType::RETURN},
    {"break", TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
    {"when", TokenType::WHEN},
    {"async", TokenType::ASYNC},
    {"await", TokenType::AWAIT},
    {"try", TokenType::TRY},
    {"catch", TokenType::CATCH},
    {"finally", TokenType::FINALLY},
    {"int", TokenType::INT},
    {"string", TokenType::STRING},
    {"bool", TokenType::BOOL},
    {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},
    {"void", TokenType::VOID},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
};

Lexer::Lexer(const string& source, ExceptionReporter& reporter) : source(source), reporter(reporter) {}

vector<Token> Lexer::tokenize() {
    while (!isAtEnd()) {
        try {
            if (atLineStart && peek() != '\n') {
                handleIndentation();
            }
            scanToken();
        } catch (const runtime_error &e) {
            // Error already reported, try to continue
            advance();
        }
    }

    if (!tokens.empty() && tokens.back().type != TokenType::NEWLINE) {
        addToken(TokenType::NEWLINE, "\\n");
    }

    while (indentStack.size() > 1) {
        indentStack.pop_back();
        addToken(TokenType::DEDENT, "DEDENT");
    }

    addToken(TokenType::EOF_TOKEN, "");
    return tokens;
}

bool Lexer::isAtEnd() const {
    return current >= source.length();
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[current];
}

char Lexer::peekNext() const {
    if (current + 1 >= source.length()) return '\0';
    return source[current + 1];
}

char Lexer::advance() {
    char c = source[current++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }

    return c;
}

void Lexer::addToken(TokenType type, const string& lexeme) {
    tokens.emplace_back(type, lexeme, line, column - lexeme.length());
}

void Lexer::scanToken() {
    char c = advance();

    switch (c) {
        case ' ':
        case '\r':
        case '\t':
            // Skip whitespace only if not at line start
            if (!atLineStart) {
                // Just skip
            } else {
                // Don't skip - handleIndentation() will process it
                current--;  // Go back
                column--;
                handleIndentation();
            }
            break;

        case '\n':
            addToken(TokenType::NEWLINE, "\\n");
            atLineStart = true;
            currentIndent = 0;
            break;

        case '+':
            if (peek() == '=') {
                advance();
                addToken(TokenType::PLUS_EQUAL, "+=");
            }
            else {
                addToken(TokenType::PLUS, "+");
            }
            break;

        case '-':
            if (peek() == '>') {
                advance();
                addToken(TokenType::ARROW, "->");
            } else if (peek() == '=') {
                advance();
                addToken(TokenType::MINUS_EQUAL, "-=");
            } else {
                addToken(TokenType::MINUS, "-");
            }
            break;

        case '&':
            if (peek() == '&') {
                advance();
                addToken(TokenType::AND, "&&");
            }
            break;

        case '|':
            if (peek() == '|') {
                advance();
                addToken(TokenType::OR, "||");
            }
            break;

        case '*':
            if (peek() == '*') {
                advance();
                addToken(TokenType::STAR_EQUAL, "*=");
            } else {
                addToken(TokenType::STAR, "*");
            }
            break;

        case '%':
            addToken(TokenType::PERCENT, "%");
            break;

        case '/':
            if (peek() == '/') {
                // Single line comment.
                scanComment();
            } else if (peek() == '*') {
                advance();
                while (!isAtEnd() && !(peek() == '*' && peekNext() == '/')) {
                    advance();
                }
                advance();
                advance();
            } else if (peek() == '=') {
                advance();
                addToken(TokenType::SLASH_EQUAL, "/=");
            } else {
                addToken(TokenType::SLASH, "/");
            }
            break;

        case '<':
            if (peek() == '=') {
                advance();
                addToken(TokenType::LESS_EQUAL, "<=");
            } else {
                addToken(TokenType::LESS, "<");
            }
            break;

        case '>':
            if (peek() == '=') {
                advance();
                addToken(TokenType::GREATER_EQUAL, ">=");
            } else {
                addToken(TokenType::GREATER, ">");
            }
            break;

        case '!':
            if (peek() == '=') {
                advance();
                addToken(TokenType::BANG_EQUAL, "!=");
            }
            break;

        case '(':
            addToken(TokenType::LEFT_PAREN, "(");
            break;

        case ')':
            addToken(TokenType::RIGHT_PAREN, ")");
            break;

        case '{':
            addToken(TokenType::LEFT_BRACE, "{");
            break;

        case '}':
            addToken(TokenType::RIGHT_BRACE, "}");
            break;

        case '[':
            addToken(TokenType::LEFT_BRACKET, "[");
            break;

        case ']':
            addToken(TokenType::RIGHT_BRACKET, "]");
            break;

        case ',':
            addToken(TokenType::COMMA, ",");
            break;

        case '.':
            addToken(TokenType::DOT, ".");
            break;

        case ':':
            if (peek() == ':') {
                advance();
                addToken(TokenType::DOUBLE_COLON, "::");
            } else {
                addToken(TokenType::COLON, ":");
            }
            break;

        case '=':
            if (peek() == '=') {
                advance();
                addToken(TokenType::EQUAL_EQUAL, "==");
            } else if (peek() == '>') {
                advance();
                addToken(TokenType::FAT_ARROW, "=>");
            } else {
                addToken(TokenType::EQUAL, "=");
            }
            break;

        case '"':
            scanString();
            break;

        case '#':
            scanComment();
            break;

        default:
            if (isDigit(c)) {
                current--; // Go back
                column--;
                scanNumber();
            } else if (isAlpha(c)) {
                current--;
                column--;
                scanIdentifier();
            } else {
                addToken(TokenType::UNKNOWN, std::string(1, c));
            }
            break;
    }
}

void Lexer::scanNumber() {
    size_t start = current;

    while (isDigit(peek())) {
        advance();
    }

    // Check for decimal
    bool isFloat = false;
    if (peek() == '.' && isDigit(peekNext())) {
        isFloat = true;
        advance();
        while (isDigit(peek())) {
            advance();
        }
    }

    string number = source.substr(start, current - start);
    addToken(isFloat ? TokenType::FLOAT_LITERAL : TokenType::INTEGER_LITERAL, number);
}

void Lexer::scanString() {
    size_t start = current;

    while (peek() != '"' && !isAtEnd()) {
        advance();
    }

    if (isAtEnd()) {
        int length = current - start;
        SourceLocation location(line, column - length, length);
        reporter.error(location, "Unterminated string literal");
        throw runtime_error("Unterminated string");
    }

    advance(); // closing "

    string str = source.substr(start, current - start - 1);
    addToken(TokenType::STRING_LITERAL, str);
}

void Lexer::scanIdentifier() {
    size_t start = current;

    while (isAlphaNumeric(peek())) {
        advance();
    }

    string text = source.substr(start, current - start);

    // Check if keyword
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        addToken(it->second, text);
    } else {
        addToken(TokenType::IDENTIFIER, text);
    }
}

void Lexer::scanComment() {
    while (peek() != '\n' && !isAtEnd()) {
        advance();
    }
}

void Lexer::handleIndentation() {
    if (!atLineStart) return;

    int spaces = 0;

    // Count leading spaces/tabs
    while (peek() == ' ' || peek() == '\t') {
        if (peek() == '\t') {
            spaces += 4;  // Tab = 4 spaces
        } else {
            spaces += 1;
        }
        advance();
    }

    // Skip empty lines and comments
    if (peek() == '\n' || peek() == '#') {
        return;
    }

    currentIndent = spaces;
    atLineStart = false;

    // Compare with previous indent level
    int previousIndent = indentStack.back();

    if (currentIndent > previousIndent) {
        // INDENT: Deeper nesting
        indentStack.push_back(currentIndent);
        addToken(TokenType::INDENT, "INDENT");
    } else if (currentIndent < previousIndent) {
        // DEDENT: Coming back out
        while (!indentStack.empty() && indentStack.back() > currentIndent) {
            indentStack.pop_back();
            addToken(TokenType::DEDENT, "DEDENT");
        }

        // Check for indentation error
        if (indentStack.empty() || indentStack.back() != currentIndent) {
            SourceLocation location(line, 1, currentIndent);
            reporter.error(location, "Inconsistent indentation");
            throw runtime_error("Indentation exception");
        }
    }
}

bool Lexer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlpha(char c) const {
    return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_';
}

bool Lexer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

// ======================
// LLVM Code Generator
// ======================

class CodeGenerator {
public:
    CodeGenerator()
        : context(make_unique<LLVMContext>()),
          builder(make_unique<IRBuilder<>>(*context)),
          module(make_unique<Module>("mylang", *context)) {
        // Initialize LLVM
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        InitializeNativeTargetAsmParser();
    }

    void declarePrintf() {
        // Deklariere printf: int printf(char*, ...)
        FunctionType* printfType = FunctionType::get(
            Type::getInt32Ty(*context),
            {PointerType::get(Type::getInt8Ty(*context), 0)},
            true
        );

        Function::Create(
            printfType,
            Function::ExternalLinkage,
            "printf",
            module.get()
        );
    }

    void declareFunction(FunctionDeclaration* funcDecl) {
        // Build parameter types
        vector<Type*> paramTypes;
        for (const auto& param : funcDecl->parameters) {
            paramTypes.push_back(getType(param.type));
        }

        // Build function type
        Type* returnType;
        if (funcDecl->name == "main") {
            returnType = Type::getInt32Ty(*context);
        } else {
            returnType = getType(funcDecl->returnType);
        }
        FunctionType* funcType = FunctionType::get(returnType, paramTypes, false);

        string mangledName = mangleFunctionName(funcDecl->name, funcDecl->parameters);

        if (funcDecl->name != "main") {
            mangledName = "c_" + mangledName;
        }

        // Create function (without body)
        Function* function = Function::Create(
            funcType,
            Function::ExternalLinkage,
            mangledName,
            module.get()
        );

        // Store in function table
        string tableName = mangleFunctionName(funcDecl->name, funcDecl->parameters);
        functions[tableName] = function;
    }

    void declareStringFunctions() {
        // strlen
        FunctionType* strlenType = FunctionType::get(
            Type::getInt64Ty(*context),
            {PointerType::get(Type::getInt8Ty(*context), 0)},
            false
        );
        Function::Create(strlenType, Function::ExternalLinkage, "strlen", module.get());

        // malloc
        FunctionType* mallocType = FunctionType::get(
            PointerType::get(Type::getInt8Ty(*context), 0),
            {Type::getInt64Ty(*context)},
            false
        );
        Function::Create(mallocType, Function::ExternalLinkage, "malloc", module.get());

        // strcpy
        FunctionType* strcpyType = FunctionType::get(
            PointerType::get(Type::getInt8Ty(*context), 0),
            {
                PointerType::get(Type::getInt8Ty(*context), 0),
                PointerType::get(Type::getInt8Ty(*context), 0)
            },
            false
        );
        Function::Create(strcpyType, Function::ExternalLinkage, "strcpy", module.get());

        // strcat
        FunctionType* strcatType = FunctionType::get(
            PointerType::get(Type::getInt8Ty(*context), 0),
            {
                PointerType::get(Type::getInt8Ty(*context), 0),
                PointerType::get(Type::getInt8Ty(*context), 0)
            },
            false
        );
        Function::Create(strcatType, Function::ExternalLinkage, "strcat", module.get());

        // sprintf (für int/bool zu string conversion)
        FunctionType* sprintfType = FunctionType::get(
            Type::getInt32Ty(*context),
            {
                PointerType::get(Type::getInt8Ty(*context), 0),
                PointerType::get(Type::getInt8Ty(*context), 0)
            },
            true  // varargs
        );
        Function::Create(sprintfType, Function::ExternalLinkage, "sprintf", module.get());
    }

    void generate(const Program& program) {
        declarePrintf();
        declareStringFunctions();

        // Step 1: Collect all functions (signatures only)
        for (const auto& stmt : program.statements) {
            if (auto* funcDecl = dynamic_cast<FunctionDeclaration*>(stmt.get())) {
                declareFunction(funcDecl);
            }
        }

        // Step 2: Generate function bodies
        for (const auto& stmt : program.statements) {
            generateStatement(stmt.get());
        }
    }

    void printIR() {
        module->print(outs(), nullptr);
    }

    void writeObjectFile(const string& filename) {
        // Get target triple
        auto targetTriple = sys::getDefaultTargetTriple();
        module->setTargetTriple(Triple(targetTriple));

        string error;
        auto target = TargetRegistry::lookupTarget(targetTriple, error);

        if (!target) {
            errs() << "Error: " << error << "\n";
            return;
        }

        auto CPU = "generic";
        auto features = "";

        TargetOptions opt;
        auto machine = target->createTargetMachine(
            Triple(targetTriple), CPU, features, opt, std::nullopt
        );

        module->setDataLayout(machine->createDataLayout());

        // Open output file
        error_code EC;
        raw_fd_ostream dest(filename, EC, sys::fs::OF_None);

        if (EC) {
            errs() << "Could not open file: " << EC.message() << "\n";
            return;
        }

        // Emit object file
        legacy::PassManager pass;
        auto fileType = CodeGenFileType::ObjectFile;

        if (machine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
            errs() << "Target machine can't emit object file\n";
            return;
        }

        pass.run(*module);
        dest.flush();
    }

private:
    unique_ptr<LLVMContext> context;
    unique_ptr<IRBuilder<> > builder;
    unique_ptr<Module> module;

    // Symbol table: variable name -> LLVM Value*
    unordered_map<string, Value *> namedValues;

    // Function table: function name -> LLVM Function*
    unordered_map<string, Function *> functions;

    // Current function being compiled
    Function *currentFunction = nullptr;

    // ========================================================================
    // Type Conversion
    // ========================================================================

    Type *getType(const string &typeName) {
        if (typeName == "int") {
            return Type::getInt32Ty(*context);
        } else if (typeName == "bool") {
            return Type::getInt1Ty(*context);
        } else if (typeName == "float") {
            return Type::getDoubleTy(*context);
        } else if (typeName == "void") {
            return Type::getVoidTy(*context);
        } else if (typeName == "string") {
            return PointerType::getUnqual(*context);
        }

        // Default to int
        return Type::getInt32Ty(*context);
    }

    string mangleFunctionName(const string& name, const vector<Parameter>& parameters) {
        // Never mangle main function.
        if (name == "main") {
            return "main";
        }

        string mangledName = name;

        for (const auto& parameter : parameters) {
            mangledName += "_" + parameter.type;
        }

        // Add "_void" if no parameter is defined.
        if (parameters.empty()) {
            mangledName += "_void";
        }

        return mangledName;
    }

    string mangleFunctionName(const string& name, const vector<string>& paramTypes) {
        if (name == "main") {
            return "main";
        }

        string mangledName = name;

        for (const auto& type : paramTypes) {
            mangledName += "_" + type;
        }

        // Add "_void" if no param type was defined.
        if (paramTypes.empty()) {
            mangledName += "_void";
        }

        return mangledName;
    }

    string getExpressionType(Expression* expr) {
        if (dynamic_cast<IntLiteral*>(expr)) {
            return "int";
        }
        else if (dynamic_cast<BoolLiteral*>(expr)) {
            return "bool";
        }
        else if (dynamic_cast<::StringLiteral*>(expr)) {
            return "string";
        }
        else if (auto* var = dynamic_cast<Variable*>(expr)) {
            // Schaue in die Symbol-Tabelle
            Value* varPtr = namedValues[var->name];
            if (!varPtr) return "int"; // Default

            if (auto* allocaInst = dyn_cast<AllocaInst>(varPtr)) {
                Type* type = allocaInst->getAllocatedType();
                if (type->isIntegerTy(32)) return "int";
                if (type->isIntegerTy(1)) return "bool";
                if (type->isDoubleTy()) return "float";
                if (type->isPointerTy()) return "string";
            }
        }
        else if (auto* binOp = dynamic_cast<BinaryOperation*>(expr)) {
            // Meiste binäre Operatoren geben den Typ der Operanden zurück
            return getExpressionType(binOp->left.get());
        }

        return "int"; // Default
    }

    // ========================================================================
    // Statement Generation
    // ========================================================================

    void generateStatement(Statement *stmt) {
        if (auto *funcDecl = dynamic_cast<FunctionDeclaration *>(stmt)) {
            generateFunction(funcDecl);
            return;
        }

        BasicBlock* currentBlock = builder->GetInsertBlock();
        if (currentBlock && currentBlock->getTerminator()) {
            return;
        }

        if (auto *varDecl = dynamic_cast<VariableDeclaration *>(stmt)) {
            generateVariableDeclaration(varDecl);
        } else if (auto *returnStmt = dynamic_cast<ReturnStatement *>(stmt)) {
            generateReturn(returnStmt);
        } else if (auto *ifStmt = dynamic_cast<IfStatement *>(stmt)) {
            generateIf(ifStmt);
        } else if (auto *whileStmt = dynamic_cast<WhileStatement *>(stmt)) {
            generateWhile(whileStmt);
        } else if (auto *exprStmt = dynamic_cast<ExpressionStatement *>(stmt)) {
            generateExpression(exprStmt->expression.get());
        }
    }

    // ========================================================================
    // Function Generation
    // ========================================================================

    void generateFunction(FunctionDeclaration* funcDecl) {
        // Get the already-declared function
        string mangledName = mangleFunctionName(funcDecl->name, funcDecl->parameters);
        Function* function = functions[mangledName];
        currentFunction = function;

        // Create entry block
        BasicBlock* entryBlock = BasicBlock::Create(*context, "entry", function);
        builder->SetInsertPoint(entryBlock);

        // Create allocas for parameters and store initial values
        for (auto& arg : function->args()) {
            // Set parameter name
            arg.setName(funcDecl->parameters[arg.getArgNo()].name);

            // Create alloca for this parameter
            AllocaInst* alloca = builder->CreateAlloca(
                arg.getType(),
                nullptr,
                arg.getName()
            );

            // Store the parameter value
            builder->CreateStore(&arg, alloca);

            // Add to symbol table
            namedValues[std::string(arg.getName())] = alloca;
        }

        // Generate function body
        for (const auto& stmt : funcDecl->body) {
            generateStatement(stmt.get());
        }

        // Add default return if missing
        BasicBlock* currentBlock = builder->GetInsertBlock();
        if (currentBlock && !currentBlock->getTerminator()) {
            if (funcDecl->name == "main") {
                builder->CreateRet(ConstantInt::get(*context, APInt(32, 0)));
            } else if (function->getReturnType()->isVoidTy()) {
                builder->CreateRetVoid();
            } else {
                builder->CreateUnreachable();
            }
        }

        // Verify function
        verifyFunction(*function, &errs());

        // Clear local symbol table
        namedValues.clear();
        currentFunction = nullptr;
    }

    void generateVariableDeclaration(VariableDeclaration* varDecl) {
        // Create alloca instruction in entry block
        Function* function = builder->GetInsertBlock()->getParent();
        IRBuilder<> tmpBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());

        Type* type = nullptr;
        Value* initValue = nullptr;

        // Generate initializer first if present
        if (varDecl->initializer) {
            initValue = generateExpression(varDecl->initializer.get());

            // If no type specified, infer from initializer
            if (varDecl->type.empty()) {
                type = initValue->getType();
            } else {
                type = getType(varDecl->type);
            }
        } else {
            // No initializer, must have explicit type
            type = getType(varDecl->type.empty() ? "int" : varDecl->type);

            // Default value
            if (type->isIntegerTy(32)) {
                initValue = ConstantInt::get(*context, APInt(32, 0));
            } else if (type->isIntegerTy(1)) {
                initValue = ConstantInt::get(*context, APInt(1, 0));
            } else if (type->isPointerTy()) {
                initValue = ConstantPointerNull::get(cast<PointerType>(type));
            }
        }

        AllocaInst* alloca = tmpBuilder.CreateAlloca(type, nullptr, varDecl->name);

        if (initValue) {
            builder->CreateStore(initValue, alloca);
        }

        // Store the alloca in symbol table
        namedValues[varDecl->name] = alloca;
    }

    // ========================================================================
    // Return Statement
    // ========================================================================

    void generateReturn(ReturnStatement* returnStmt) {
        if (returnStmt->value) {
            Value* retValue = generateExpression(returnStmt->value.get());
            builder->CreateRet(retValue);
        } else {
            builder->CreateRetVoid();
        }
    }

    // ========================================================================
    // If Statement
    // ========================================================================

    void generateIf(IfStatement *ifStmt) {
        Value *condition = generateExpression(ifStmt->condition.get());

        // Convert to boolean if needed
        if (!condition->getType()->isIntegerTy(1)) {
            condition = builder->CreateICmpNE(
                condition,
                ConstantInt::get(*context, APInt(32, 0)),
                "ifcond"
            );
        }

        Function *function = builder->GetInsertBlock()->getParent();

        // Create blocks
        BasicBlock *thenBB = BasicBlock::Create(*context, "then", function);
        BasicBlock *elseBB = nullptr;
        BasicBlock *mergeBB = nullptr;

        // Nur merge-Block erstellen wenn nötig
        bool needsMerge = true;

        if (!ifStmt->elseBranch.empty()) {
            elseBB = BasicBlock::Create(*context, "else");
            builder->CreateCondBr(condition, thenBB, elseBB);
        } else {
            mergeBB = BasicBlock::Create(*context, "ifcont");
            builder->CreateCondBr(condition, thenBB, mergeBB);
            needsMerge = true; // Wir haben schon einen merge-Block
        }

        // Then block
        builder->SetInsertPoint(thenBB);
        for (const auto &stmt: ifStmt->thenBranch) {
            generateStatement(stmt.get());
        }
        bool thenHasTerminator = builder->GetInsertBlock()->getTerminator() != nullptr;

        // Else block
        bool elseHasTerminator = false;
        if (!ifStmt->elseBranch.empty()) {
            function->insert(function->end(), elseBB);
            builder->SetInsertPoint(elseBB);
            for (const auto &stmt: ifStmt->elseBranch) {
                generateStatement(stmt.get());
            }
            elseHasTerminator = builder->GetInsertBlock()->getTerminator() != nullptr;
        }

        // Merge block logic
        if (!ifStmt->elseBranch.empty()) {
            // If-else statement
            if (!thenHasTerminator || !elseHasTerminator) {
                // At least one branch needs to jump to merge
                mergeBB = BasicBlock::Create(*context, "ifcont");

                if (!thenHasTerminator) {
                    builder->SetInsertPoint(thenBB);
                    builder->CreateBr(mergeBB);
                }

                if (!elseHasTerminator) {
                    builder->SetInsertPoint(elseBB);
                    builder->CreateBr(mergeBB);
                }

                function->insert(function->end(), mergeBB);
                builder->SetInsertPoint(mergeBB);
            }
        } else {
            // Simple if without else
            if (!thenHasTerminator) {
                builder->SetInsertPoint(thenBB);
                builder->CreateBr(mergeBB);
            }
            function->insert(function->end(), mergeBB);
            builder->SetInsertPoint(mergeBB);
        }
    }

    // ========================================================================
    // While Statement
    // ========================================================================

    void generateWhile(WhileStatement* whileStmt) {
        Function* function = builder->GetInsertBlock()->getParent();

        BasicBlock* condBB = BasicBlock::Create(*context, "whilecond", function);
        BasicBlock* loopBB = BasicBlock::Create(*context, "whileloop");
        BasicBlock* afterBB = BasicBlock::Create(*context, "afterloop");

        // Jump to condition
        builder->CreateBr(condBB);

        // Condition block
        builder->SetInsertPoint(condBB);
        Value* condition = generateExpression(whileStmt->condition.get());

        if (!condition->getType()->isIntegerTy(1)) {
            condition = builder->CreateICmpNE(
                condition,
                ConstantInt::get(*context, APInt(32, 0)),
                "loopcond"
            );
        }

        builder->CreateCondBr(condition, loopBB, afterBB);

        // Loop body
        function->insert(function->end(), loopBB);
        builder->SetInsertPoint(loopBB);
        for (const auto& stmt : whileStmt->body) {
            generateStatement(stmt.get());
        }
        builder->CreateBr(condBB);

        // After loop
        function->insert(function->end(), afterBB);
        builder->SetInsertPoint(afterBB);
    }

    // ========================================================================
    // Expression Generation
    // ========================================================================

    Value* generateExpression(Expression* expr) {
        if (auto* intLit = dynamic_cast<IntLiteral*>(expr)) {
            return ConstantInt::get(*context, APInt(32, intLit->value));
        }
        else if (auto* boolLit = dynamic_cast<BoolLiteral*>(expr)) {
            return ConstantInt::get(*context, APInt(1, boolLit->value ? 1 : 0));
        }
        else if (auto* strLit = dynamic_cast<::StringLiteral*>(expr)) {
            return builder->CreateGlobalStringPtr(strLit->value);
        }
        else if (auto* var = dynamic_cast<Variable*>(expr)) {
            Value* varPtr = namedValues[var->name];
            if (!varPtr) {
                errs() << "Unknown variable: " << var->name << "\n";
                return nullptr;
            }

            // Load the value from memory
            // In LLVM 21+ müssen wir den Typ aus der AllocaInst holen
            if (auto* allocaInst = dyn_cast<AllocaInst>(varPtr)) {
                return builder->CreateLoad(
                    allocaInst->getAllocatedType(),
                    varPtr,
                    var->name.c_str()
                );
            } else {
                // Falls es ein Argument ist (sollte nicht vorkommen nach unseren Änderungen)
                return varPtr;
            }
        }
        else if (auto* unaryOp = dynamic_cast<UnaryOperation*>(expr)) {
            return generateUnaryOp(unaryOp);
        }
        else if (auto* binOp = dynamic_cast<BinaryOperation*>(expr)) {
            return generateBinaryOp(binOp);
        }
        else if (auto* call = dynamic_cast<FunctionCall*>(expr)) {
            return generateFunctionCall(call);
        }
        else if (auto* assign = dynamic_cast<Assignment*>(expr)) {
            Value* val = generateExpression(assign->value.get());
            Value* varPtr = namedValues[assign->name];
            if (!varPtr) {
                errs() << "Unknown variable: " << assign->name << "\n";
                return nullptr;
            }
            builder->CreateStore(val, varPtr);
            return val;
        }

        return nullptr;
    }

    Value* generateStringConcatenation(BinaryOperation *binOp) {
        Value* left = generateExpression(binOp->left.get());
        Value* right = generateExpression(binOp->right.get());

        if (!left || !right) return nullptr;

        // Convert to string if needed
        left = convertToString(left);
        right = convertToString(right);

        if (!left || !right) return nullptr;

        // Get string functions
        Function *strlenFunc = module->getFunction("strlen");
        Function *mallocFunc = module->getFunction("malloc");
        Function *strcpyFunc = module->getFunction("strcpy");
        Function *strcatFunc = module->getFunction("strcat");

        // Calculate total length: len(left) + len(right) + 1
        Value *len1 = builder->CreateCall(strlenFunc, {left}, "len1");
        Value *len2 = builder->CreateCall(strlenFunc, {right}, "len2");
        Value *totalLen = builder->CreateAdd(len1, len2, "totallen");
        totalLen = builder->CreateAdd(
            totalLen,
            ConstantInt::get(*context, APInt(64, 1)),
            "totallen_plus1"
        );

        // Allocate memory
        Value *result = builder->CreateCall(mallocFunc, {totalLen}, "concat_result");

        // Copy first string
        builder->CreateCall(strcpyFunc, {result, left});

        // Concatenate second string
        builder->CreateCall(strcatFunc, {result, right});

        return result;
    }

    Value* generateBinaryOp(BinaryOperation* binOp) {
        if (binOp->op == TokenType::AND || binOp->op == TokenType::OR) {
            return generateLogicalOp(binOp);
        }

        if (binOp->op == TokenType::DOT) {
            return generateStringConcatenation(binOp);
        }

        Value* left = generateExpression(binOp->left.get());
        Value* right = generateExpression(binOp->right.get());

        if (!left || !right) return nullptr;

        switch (binOp->op) {
            case TokenType::PLUS:
                return builder->CreateAdd(left, right, "addtmp");
            case TokenType::MINUS:
                return builder->CreateSub(left, right, "subtmp");
            case TokenType::STAR:
                return builder->CreateMul(left, right, "multmp");
            case TokenType::SLASH:
                return builder->CreateSDiv(left, right, "divtmp");
            case TokenType::PERCENT:
                return builder->CreateSRem(left, right, "modtmp");
            case TokenType::EQUAL_EQUAL:
                return builder->CreateICmpEQ(left, right, "eqtmp");
            case TokenType::BANG_EQUAL:
                return builder->CreateICmpNE(left, right, "netmp");
            case TokenType::LESS:
                return builder->CreateICmpSLT(left, right, "lttmp");
            case TokenType::LESS_EQUAL:
                return builder->CreateICmpSLE(left, right, "letmp");
            case TokenType::GREATER:
                return builder->CreateICmpSGT(left, right, "gttmp");
            case TokenType::GREATER_EQUAL:
                return builder->CreateICmpSGE(left, right, "getmp");
            default:
                errs() << "Unknown binary operator\n";
                return nullptr;
        }
    }

    Value* generateUnaryOp(UnaryOperation* unaryOp) {
        Value* operand = generateExpression(unaryOp->operand.get());
        if (!operand) return nullptr;

        switch (unaryOp->op) {
            case TokenType::MINUS:
                // Negate: 0 - operand
                if (operand->getType()->isIntegerTy()) {
                    return builder->CreateNeg(operand, "negtmp");
                } else if (operand->getType()->isDoubleTy()) {
                    return builder->CreateFNeg(operand, "negtmp");
                }
                break;
            case TokenType::NOT:
                // Logical NOT
                return builder->CreateNot(operand, "nottmp");
            default:
                errs() << "Unknown unary operator\n";
                return nullptr;
        }
        return nullptr;
    }

    Value *generateLogicalOp(BinaryOperation *binOp) {
        Function *function = builder->GetInsertBlock()->getParent();

        // Evaluate left side
        Value *left = generateExpression(binOp->left.get());
        if (!left) return nullptr;

        // Convert to i1 if needed
        if (!left->getType()->isIntegerTy(1)) {
            left = builder->CreateICmpNE(
                left,
                ConstantInt::get(*context, APInt(32, 0)),
                "tobool"
            );
        }

        BasicBlock *startBB = builder->GetInsertBlock();
        BasicBlock *rightBB = BasicBlock::Create(*context, "logical_right");
        BasicBlock *mergeBB = BasicBlock::Create(*context, "logical_merge");

        if (binOp->op == TokenType::AND) {
            // AND: only evaluate right if left is true
            builder->CreateCondBr(left, rightBB, mergeBB);
        } else {
            // OR: only evaluate right if left is false
            builder->CreateCondBr(left, mergeBB, rightBB);
        }

        // Right block
        function->insert(function->end(), rightBB);
        builder->SetInsertPoint(rightBB);
        Value *right = generateExpression(binOp->right.get());
        if (!right) return nullptr;

        // Convert to i1 if needed
        if (!right->getType()->isIntegerTy(1)) {
            right = builder->CreateICmpNE(
                right,
                ConstantInt::get(*context, APInt(32, 0)),
                "tobool"
            );
        }

        BasicBlock *rightEndBB = builder->GetInsertBlock();
        builder->CreateBr(mergeBB);

        // Merge block
        function->insert(function->end(), mergeBB);
        builder->SetInsertPoint(mergeBB);

        PHINode *phi = builder->CreatePHI(Type::getInt1Ty(*context), 2, "logical_result");

        if (binOp->op == TokenType::AND) {
            // AND: false from start, or result from right
            phi->addIncoming(ConstantInt::getFalse(*context), startBB);
            phi->addIncoming(right, rightEndBB);
        } else {
            // OR: true from start, or result from right
            phi->addIncoming(ConstantInt::getTrue(*context), startBB);
            phi->addIncoming(right, rightEndBB);
        }

        return phi;
    }

    Value* generatePrint(FunctionCall* call) {
        Function* printfFunc = module->getFunction("printf");

        if (call->arguments.empty()) {
            errs() << "print() requires at least one argument\n";
            return nullptr;
        }

        // Get the first argument
        Value* arg = generateExpression(call->arguments[0].get());
        if (!arg) return nullptr;

        Value* formatStr = nullptr;
        vector<Value*> printfArgs;

        // Determine format string based on type
        if (arg->getType()->isIntegerTy(32)) {
            // Integer
            formatStr = builder->CreateGlobalStringPtr("%d\n");
            printfArgs.push_back(formatStr);
            printfArgs.push_back(arg);
        } else if (arg->getType()->isIntegerTy(1)) {
            // Boolean - convert to i32 first for comparison
            Value* boolAsInt = builder->CreateZExt(arg, Type::getInt32Ty(*context), "booltoint");
            Value* trueStr = builder->CreateGlobalStringPtr("true");
            Value* falseStr = builder->CreateGlobalStringPtr("false");

            // Compare with 0
            Value* isTrue = builder->CreateICmpNE(
                boolAsInt,
                ConstantInt::get(*context, APInt(32, 0))
            );
            Value* selectedStr = builder->CreateSelect(isTrue, trueStr, falseStr);

            formatStr = builder->CreateGlobalStringPtr("%s\n");
            printfArgs.push_back(formatStr);
            printfArgs.push_back(selectedStr);
        } else if (arg->getType()->isDoubleTy()) {
            // Float
            formatStr = builder->CreateGlobalStringPtr("%f\n");
            printfArgs.push_back(formatStr);
            printfArgs.push_back(arg);
        } else if (arg->getType()->isPointerTy()) {
            formatStr = builder->CreateGlobalStringPtr("%s\n");
            printfArgs.push_back(formatStr);
            printfArgs.push_back(arg);
        } else {
            errs() << "Unsupported type for print()\n";
            return nullptr;
        }

        return builder->CreateCall(printfFunc, printfArgs, "printcall");
    }

    Value* generateFunctionCall(FunctionCall* call) {
        // Special handling for print()
        if (call->name == "print") {
            return generatePrint(call);
        }

        vector<string> argTypes;
        for (const auto& arg : call->arguments) {
            argTypes.push_back(getExpressionType(arg.get()));
        }

        string mangledName = mangleFunctionName(call->name, argTypes);

        Function* calleeF = functions[mangledName];
        if (!calleeF) {
            errs() << "Unknown function: " << call->name << "\n";
            return nullptr;
        }

        if (calleeF->arg_size() != call->arguments.size()) {
            errs() << "Incorrect # of arguments passed\n";
            return nullptr;
        }

        vector<Value*> args;
        for (const auto& arg : call->arguments) {
            args.push_back(generateExpression(arg.get()));
            if (!args.back()) return nullptr;
        }

        // Wenn Funktion void zurückgibt, gib nullptr zurück
        if (calleeF->getReturnType()->isVoidTy()) {
            builder->CreateCall(calleeF, args);
            return nullptr;
        }

        return builder->CreateCall(calleeF, args, "calltmp");
    }

    // Helper: Convert any value to string
    Value* convertToString(Value* val) {
        if (!val) return nullptr;

        // Already a string pointer
        if (val->getType()->isPointerTy()) {
            return val;
        }

        Function* mallocFunc = module->getFunction("malloc");
        Function* sprintfFunc = module->getFunction("sprintf");

        // Allocate buffer (32 bytes should be enough for int/bool/float)
        Value* buffer = builder->CreateCall(
            mallocFunc,
            {ConstantInt::get(*context, APInt(64, 32))},
            "str_buffer"
        );

        if (val->getType()->isIntegerTy(32)) {
            // Convert int to string
            Value* format = builder->CreateGlobalStringPtr("%d");
            builder->CreateCall(sprintfFunc, {buffer, format, val});
            return buffer;
        }
        else if (val->getType()->isIntegerTy(1)) {
            // Convert bool to string
            Value* boolAsInt = builder->CreateZExt(val, Type::getInt32Ty(*context));
            Value* trueStr = builder->CreateGlobalStringPtr("true");
            Value* falseStr = builder->CreateGlobalStringPtr("false");
            Value* isTrue = builder->CreateICmpNE(
                boolAsInt,
                ConstantInt::get(*context, APInt(32, 0))
            );
            return builder->CreateSelect(isTrue, trueStr, falseStr);
        }
        else if (val->getType()->isDoubleTy()) {
            // Convert float to string
            Value* format = builder->CreateGlobalStringPtr("%.2f");
            builder->CreateCall(sprintfFunc, {buffer, format, val});
            return buffer;
        }

        errs() << "Cannot convert type to string\n";
        return nullptr;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: mylang <file.ml> [-o output]" << endl;
        return 1;
    }

    string inputFile = argv[1];
    string outputFile = "output.o";
    string executableFile = "a.out";
    bool debug = false;
    bool run = true;

    // Parse command line options
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

    // Read source file
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
        // Lexer
        Lexer lexer(source, reporter);
        auto tokens = lexer.tokenize();

        if (reporter.hasError()) {
            return 1;
        }

        if (debug) {
            cout << "=== TOKENS ===" << endl;
            for (const auto& token : tokens) {
                cout << token << endl;
            }
            cout << endl;
        }

        // Parser
        Parser parser(tokens, reporter);
        auto program = parser.parse();

        if (reporter.hasError()) {
            return 1;
        }

        if (debug) {
            cout << "=== AST ===" << endl;
            program->print();
            cout << endl;
        }

        // Code Generator
        if (debug) {
            cout << "=== LLVM IR ===" << endl;
        }

        CodeGenerator codegen;
        codegen.generate(*program);

        if (debug) {
            codegen.printIR();
            cout << endl;
        }

        // Write object file
        codegen.writeObjectFile(outputFile);

        if (run) {
            // Link with clang
            string linkCommand = "clang " + outputFile + " -o " + executableFile + " 2>/dev/null";
            int linkResult = system(linkCommand.c_str());

            if (linkResult != 0) {
                cerr << "Linking failed!" << endl;
                return 1;
            }

            // Execute
            string execCommand = "./" + executableFile;
            int execResult = system(execCommand.c_str());

            // Cleanup
            remove(outputFile.c_str());
            remove(executableFile.c_str());

            // Return the exit code from the program
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