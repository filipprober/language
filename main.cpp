#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <memory>

#include "lexer/Token.h"
#include "ast/AstNode.h"

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

// ======================
// Type System (NEU - ZENTRAL)
// ======================
struct SourceLocation
{
    int line;
    int column;
    int length;

    SourceLocation(int line, int column, int length) :
        line(line),
        column(column),
        length(length) {
    }
};



// Forward declarations
struct SourceLocation;
class ExceptionReporter;

class ExceptionReporter
{
public:
    ExceptionReporter(const string& source, const string& filename) :
        source(source),
        filename(filename),
        hasErrors(false) {
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

        int lineNumWidth = to_string(location.line).length();
        string padding(lineNumWidth, ' ');

        cerr << padding << " |" << endl;
        cerr << location.line << " | " << sourceLine << endl;
        cerr << padding << " | ";

        for (int i = 0; i < location.column - 1; i++) {
            cerr << " ";
        }

        cerr << "\033[1;31m";
        for (int i = 0; i < location.length; i++) {
            cerr << "^";
        }
        cerr << "\033[0m" << endl;
        cerr << endl;
    }
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
    }

    Token(TokenType type, const string& lexeme, SourceLocation location) :
        type(type),
        lexeme(lexeme),
        location(location)
    {
    }
};

ostream& operator<<(ostream& os, const Token& token);

class ASTNode;
class Expression;
class Statement;

class Parameter {
public:
    string name;
    string typeStr;
    shared_ptr<MyType> resolvedType;  // UPDATED

    Parameter(const string& name, const string& type) :
        name(name),
        typeStr(type),
        resolvedType(MyType::parse(type)) {
    }
};

class FunctionDeclaration : public Statement {
public:
    string name;
    vector<Parameter> parameters;
    string returnTypeStr;
    shared_ptr<MyType> resolvedReturnType;  // UPDATED
    vector<unique_ptr<Statement>> body;

    FunctionDeclaration(const string& name, vector<Parameter> parameters, const string& returnType, vector<unique_ptr<Statement>> body) :
        name(name),
        parameters(std::move(parameters)),
        returnTypeStr(returnType),
        resolvedReturnType(MyType::parse(returnType)),
        body(std::move(body)) {
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "FunctionDeclaration(name=" << name;

        cout << ", params=[";
        for (size_t i = 0; i < parameters.size(); i++) {
            if (i > 0) cout << ", ";
            cout << parameters[i].name << ": " << parameters[i].typeStr;
        }
        cout << "]";

        if (!returnTypeStr.empty()) {
            cout << ", returns=" << returnTypeStr;
        }
        cout << ")" << endl;

        for (const auto& statement : body) {
            statement->print(indent + 2);
        }
    }
};

class FunctionCall : public Expression {
public:
    string name;
    vector<unique_ptr<Expression>> arguments;

    FunctionCall(const string& name, vector<unique_ptr<Expression>> arguments) :
        name(name),
        arguments(std::move(arguments)) {
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "FunctionCall(" << name << ")" << endl;
        for (const auto& argument : arguments) {
            argument->print(indent + 2);
        }
    }
};

class IfStatement : public Statement {
public:
    unique_ptr<Expression> condition;
    vector<unique_ptr<Statement>> thenBranch;
    vector<unique_ptr<Statement>> elseBranch;

    IfStatement(
        unique_ptr<Expression> condition,
        vector<unique_ptr<Statement> > thenBranch,
        vector<unique_ptr<Statement> > elseBranch
    ) : condition(std::move(condition)),
        thenBranch(std::move(thenBranch)),
        elseBranch(std::move(elseBranch)) {
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

class ReturnPathAnalyzer
{
public:
    static bool hasReturnOnAllPaths(const vector<unique_ptr<Statement>>& statements) {
        for (size_t i = 0; i < statements.size(); i++) {
            const auto& stmt = statements[i];

            if (dynamic_cast<ReturnStatement*>(stmt.get())) {
                return true;
            }

            if (auto* ifStmt = dynamic_cast<IfStatement*>(stmt.get())) {
                if (!ifStmt->elseBranch.empty()) {
                    bool thenReturns = hasReturnOnAllPaths(ifStmt->thenBranch);
                    bool elseReturns = hasReturnOnAllPaths(ifStmt->elseBranch);

                    if (thenReturns && elseReturns) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    static void validateFunction(const FunctionDeclaration* funcDecl) {
        string funcName = funcDecl->name;

        if (funcName == "main") {
            return;
        }

        if (funcDecl->resolvedReturnType->kind == TypeKind::Void) {
            return;
        }

        if (!hasReturnOnAllPaths(funcDecl->body)) {
            throw runtime_error(
                "Function '" + funcName + "' with return type '" + funcDecl->returnTypeStr +
                "' does not return a value on all code paths"
            );
        }
    }
};

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
// Type Checker (NEU)
// ======================

class TypeChecker {
public:
    TypeChecker(ExceptionReporter& reporter) : reporter(reporter) {}

    void check(Program* program) {
        // Pass 1: Sammle alle Funktionen ZUERST
        for (const auto& stmt : program->statements) {
            if (auto* funcDecl = dynamic_cast<FunctionDeclaration*>(stmt.get())) {
                collectFunction(funcDecl);
            }
        }

        // Pass 2: Type-check alle Statements
        for (const auto& stmt : program->statements) {
            checkStatement(stmt.get());
        }
    }

    unordered_map<string, shared_ptr<MyType>>& getSymbolTable() {
        return symbolTable;
    }

private:
    ExceptionReporter& reporter;
    unordered_map<string, shared_ptr<MyType>> symbolTable;

    struct FunctionSignature {
        vector<shared_ptr<MyType>> paramTypes;
        shared_ptr<MyType> returnType;
    };
    unordered_map<string, FunctionSignature> functionTable;

    void collectFunction(FunctionDeclaration* funcDecl) {
        FunctionSignature sig;
        for (const auto& param : funcDecl->parameters) {
            sig.paramTypes.push_back(param.resolvedType);
        }
        sig.returnType = funcDecl->resolvedReturnType;

        functionTable[funcDecl->name] = sig;
    }

    void checkStatement(Statement* stmt) {
        if (auto* funcDecl = dynamic_cast<FunctionDeclaration*>(stmt)) {
            checkFunction(funcDecl);
        }
        else if (auto* varDecl = dynamic_cast<VariableDeclaration*>(stmt)) {
            checkVarDeclaration(varDecl);
        }
        else if (auto* ret = dynamic_cast<ReturnStatement*>(stmt)) {
            if (ret->value) {
                inferType(ret->value.get());
            }
        }
        else if (auto* ifStmt = dynamic_cast<IfStatement*>(stmt)) {
            inferType(ifStmt->condition.get());
            for (const auto& s : ifStmt->thenBranch) checkStatement(s.get());
            for (const auto& s : ifStmt->elseBranch) checkStatement(s.get());
        }
        else if (auto* whileStmt = dynamic_cast<WhileStatement*>(stmt)) {
            inferType(whileStmt->condition.get());
            for (const auto& s : whileStmt->body) checkStatement(s.get());
        }
        else if (auto* exprStmt = dynamic_cast<ExpressionStatement*>(stmt)) {
            inferType(exprStmt->expression.get());
        }
    }

    void checkFunction(FunctionDeclaration* funcDecl) {
        // Parameters in symbol table
        for (const auto& param : funcDecl->parameters) {
            symbolTable[param.name] = param.resolvedType;
        }

        // Check body
        for (const auto& stmt : funcDecl->body) {
            checkStatement(stmt.get());
        }

        // Clear parameters
        for (const auto& param : funcDecl->parameters) {
            symbolTable.erase(param.name);
        }
    }

    void checkVarDeclaration(VariableDeclaration* varDecl) {
        if (varDecl->initializer) {
            inferType(varDecl->initializer.get());

            if (!varDecl->typeStr.empty()) {
                varDecl->resolvedType = MyType::parse(varDecl->typeStr);
            } else {
                varDecl->resolvedType = varDecl->initializer->exprType;
            }
        } else {
            varDecl->resolvedType = MyType::parse(varDecl->typeStr);
        }

        symbolTable[varDecl->name] = varDecl->resolvedType;
    }

    shared_ptr<MyType> inferType(Expression* expr) {
        if (expr->exprType) {
            return expr->exprType;  // Already inferred
        }

        if (auto* var = dynamic_cast<Variable*>(expr)) {
            auto it = symbolTable.find(var->name);
            if (it != symbolTable.end()) {
                expr->exprType = it->second;
            } else {
                expr->exprType = MyType::Int();  // Default fallback
            }
        }
        else if (auto* binOp = dynamic_cast<BinaryOperation*>(expr)) {
            auto leftType = inferType(binOp->left.get());
            auto rightType = inferType(binOp->right.get());

            if (binOp->op == TokenType::DOT) {
                expr->exprType = MyType::String();
            }
            else if (binOp->op == TokenType::EQUAL_EQUAL || binOp->op == TokenType::BANG_EQUAL ||
                     binOp->op == TokenType::LESS || binOp->op == TokenType::LESS_EQUAL ||
                     binOp->op == TokenType::GREATER || binOp->op == TokenType::GREATER_EQUAL ||
                     binOp->op == TokenType::AND || binOp->op == TokenType::OR) {
                expr->exprType = MyType::Bool();
            }
            else {
                expr->exprType = leftType;
            }
        }
        else if (auto* unaryOp = dynamic_cast<UnaryOperation*>(expr)) {
            auto operandType = inferType(unaryOp->operand.get());

            if (unaryOp->op == TokenType::NOT) {
                expr->exprType = MyType::Bool();
            } else {
                expr->exprType = operandType;
            }
        }
        else if (auto* call = dynamic_cast<FunctionCall*>(expr)) {
            if (call->name == "print") {
                expr->exprType = MyType::Void();
            } else {
                auto it = functionTable.find(call->name);
                if (it != functionTable.end()) {
                    expr->exprType = it->second.returnType;

                    const auto& paramTypes = it->second.paramTypes;
                    for (size_t i = 0; i < call->arguments.size() && i < paramTypes.size(); i++) {
                        inferType(call->arguments[i].get());

                        // Coerce null -> Optional Type
                        if (call->arguments[i]->exprType->kind == TypeKind::Null &&
                            paramTypes[i]->kind == TypeKind::Optional) {
                            // Setze den Typ des Arguments auf den erwarteten Optional-Typ
                            call->arguments[i]->exprType = paramTypes[i];
                            }
                    }
                } else {
                    expr->exprType = MyType::Int();
                }
            }

            // Infer argument types
            for (const auto& arg : call->arguments) {
                inferType(arg.get());
            }
        }
        else if (auto* assign = dynamic_cast<Assignment*>(expr)) {
            inferType(assign->value.get());
            expr->exprType = assign->value->exprType;
        }

        if (!expr->exprType) {
            expr->exprType = MyType::Int();
        }

        return expr->exprType;
    }
};

class Parser {
public:
    explicit Parser(vector<Token> tokens, ExceptionReporter& reporter) :
        tokens(std::move(tokens)),
        current(0),
        reporter(reporter)
    {
    }

    unique_ptr<Program> parse() {
        auto program = make_unique<Program>();

        while (!isAtEnd()) {
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

        Token errorToken = previous();

        size_t pos = current - 1;
        while (pos > 0 && tokens[pos].type == TokenType::NEWLINE) {
            pos--;
        }
        if (pos > 0) {
            errorToken = tokens[pos];
        }

        string fullMessage = message;
        Token nextToken = peek();

        if (nextToken.type != TokenType::EOF_TOKEN) {
            fullMessage += ", got '" + nextToken.lexeme + "'";
        }

        reporter.error(errorToken.location, fullMessage);
        throw runtime_error("Parse error");
    }

    string parseType() {
        string baseType;

        if (match({TokenType::INT, TokenType::FLOAT, TokenType::STRING, TokenType::BOOL, TokenType::VOID})) {
            baseType = previous().lexeme;
        }
        else if (match(TokenType::IDENTIFIER)) {
            baseType = previous().lexeme;
        }
        else {
            reportError("Expected type");
        }

        bool isArray = false;
        if (match(TokenType::LEFT_BRACKET)) {
            consume(TokenType::RIGHT_BRACKET, "Expected ']'");
            isArray = true;
        }

        bool isOptional = false;
        if (match(TokenType::QUESTION)) {
            isOptional = true;
        }

        string fullType = baseType;
        if (isArray) fullType += "[]";
        if (isOptional) fullType += "?";

        return fullType;
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

    unique_ptr<Statement> parseStatement() {
        if (match(TokenType::FN)) {
            return parseFunctionDeclaration();
        }

        if (match(TokenType::VAR)) {
            return parseVarDeclaration();
        }

        if (match(TokenType::IF)) {
            return parseIfStatement();
        }

        if (match(TokenType::WHILE)) {
            return parseWhileStatement();
        }

        if (match(TokenType::RETURN)) {
            return parseReturnStatement();
        }

        return parseExpressionStatement();
    }

    unique_ptr<Statement> parseFunctionDeclaration() {
        Token name = consume(TokenType::IDENTIFIER, "Expected function name");

        consume(TokenType::LEFT_PAREN, "Expected '(' after function name");

        vector<Parameter> parameters;
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                Token paramName = consume(TokenType::IDENTIFIER, "Expected parameter name");
                consume(TokenType::COLON, "Expected ':' after parameter name");

                string paramType = parseType();
                parameters.emplace_back(paramName.lexeme, paramType);
            } while (match(TokenType::COMMA));
        }

        consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters");

        string returnType = "void";
        if (match(TokenType::ARROW)) {
            returnType = parseType();
        }

        consume(TokenType::COLON, "Expected ':' after function signature");

        while (match(TokenType::NEWLINE)) {}

        vector<unique_ptr<Statement>> body = parseBlock();

        auto funcDecl = make_unique<FunctionDeclaration>(
            name.lexeme,
            std::move(parameters),
            returnType,
            std::move(body)
        );

        ReturnPathAnalyzer::validateFunction(funcDecl.get());

        return funcDecl;
    }

    unique_ptr<Statement> parseIfStatement() {
        auto condition = parseExpression();
        consume(TokenType::COLON, "Expected ':' after if condition");

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

    unique_ptr<Statement> parseWhileStatement() {
        auto condition = parseExpression();
        consume(TokenType::COLON, "Expected ':' after while condition");

        while (match(TokenType::NEWLINE)) {}

        vector<unique_ptr<Statement> > body = parseBlock();

        return make_unique<WhileStatement>(
            std::move(condition),
            std::move(body)
        );
    }

    vector<unique_ptr<Statement>> parseBlock() {
        vector<unique_ptr<Statement>> statements;

        consume(TokenType::INDENT, "Expected indentation after ':'");

        while (match(TokenType::NEWLINE)) {}

        while (!isAtEnd()) {
            if (check(TokenType::DEDENT)) {
                advance();
                break;
            }

            while (match(TokenType::NEWLINE)) {}

            if (check(TokenType::DEDENT)) {
                advance();
                break;
            }

            if (isAtEnd()) {
                break;
            }

            statements.push_back(parseStatement());
        }

        return statements;
    }

    unique_ptr<Statement> parseVarDeclaration() {
        Token name = consume(TokenType::IDENTIFIER, "Expected variable name");

        string type;

        if (match(TokenType::COLON)) {
            type = parseType();
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

    unique_ptr<Expression> parseExpression() {
        return parseAssignment();
    }

    unique_ptr<Expression> parseAssignment() {
        auto expr = parseLogicalOr();

        if (match(TokenType::EQUAL)) {
            auto* var = dynamic_cast<Variable*>(expr.get());
            if (!var) {
                reportError("Invalid assignment target");
            }

            string name = var->name;
            auto value = parseAssignment();
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
            auto operand = parseUnary();
            return make_unique<UnaryOperation>(op, std::move(operand));
        }

        return parsePrimary();
    }

    unique_ptr<Expression> parsePrimary() {
        if (match(TokenType::TRUE)) {
            return make_unique<BoolLiteral>(true);
        }
        if (match(TokenType::FALSE)) {
            return make_unique<BoolLiteral>(false);
        }
        if (match(TokenType::NONE)) {
            return make_unique<NullLiteral>();
        }

        if (match(TokenType::INTEGER_LITERAL)) {
            int value = stoi(previous().lexeme);
            return make_unique<IntLiteral>(value);
        }

        if (match(TokenType::STRING_LITERAL)) {
            return make_unique<MyStringLiteral>(previous().lexeme);
        }

        if (match(TokenType::INTERPOLATED_STRING)) {
            string template_str = previous().lexeme;
            vector<string> variables;

            size_t pos = 0;
            while ((pos = template_str.find('{', pos)) != string::npos) {
                size_t end = template_str.find('}', pos);
                if (end != string::npos) {
                    string varName = template_str.substr(pos + 1, end - pos - 1);
                    variables.push_back(varName);
                    pos = end + 1;
                } else {
                    break;
                }
            }

            return make_unique<InterpolatedString>(template_str, std::move(variables));
        }

        if (match(TokenType::IDENTIFIER)) {
            string name = previous().lexeme;

            if (match(TokenType::LEFT_PAREN)) {
                vector<unique_ptr<Expression>> arguments;

                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        arguments.push_back(parseExpression());
                    } while (match(TokenType::COMMA));
                }

                consume(TokenType::RIGHT_PAREN, "Expected ')' after arguments");
                return make_unique<FunctionCall>(name, std::move(arguments));
            }

            return make_unique<Variable>(name);
        }

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
        case TokenType::INT: return "INT";
        case TokenType::FLOAT: return "FLOAT";
        case TokenType::STRING: return "STRING";
        case TokenType::BOOL: return "BOOL";
        case TokenType::VOID: return "VOID";
        case TokenType::ANY: return "ANY";
        case TokenType::QUESTION: return "QUESTION";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::NONE: return "NONE";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::INTEGER_LITERAL: return "INTEGER_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::INTERPOLATED_STRING: return "INTERPOLATED_STRING";
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
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::NOT: return "NOT";
        case TokenType::IS: return "IS";
        case TokenType::AS: return "AS";
        case TokenType::IN: return "IN";
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

// Lexer bleibt unverändert...
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
    void scanInterpolatedString();
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
    {"null", TokenType::NONE},
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
            if (!atLineStart) {
            } else {
                current--;
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
            atLineStart = false;
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
            atLineStart = false;
            break;

        case '&':
            if (peek() == '&') {
                advance();
                addToken(TokenType::AND, "&&");
            }
            atLineStart = false;
            break;

        case '|':
            if (peek() == '|') {
                advance();
                addToken(TokenType::OR, "||");
            }
            atLineStart = false;
            break;

        case '*':
            if (peek() == '*') {
                advance();
                addToken(TokenType::STAR_EQUAL, "*=");
            } else {
                addToken(TokenType::STAR, "*");
            }
            atLineStart = false;
            break;

        case '%':
            addToken(TokenType::PERCENT, "%");
            atLineStart = false;
            break;

        case '?':
            addToken(TokenType::QUESTION, "?");
            atLineStart = false;
            break;

        case '/':
            if (peek() == '/') {
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
            atLineStart = false;
            break;

        case '<':
            if (peek() == '=') {
                advance();
                addToken(TokenType::LESS_EQUAL, "<=");
            } else {
                addToken(TokenType::LESS, "<");
            }
            atLineStart = false;
            break;

        case '>':
            if (peek() == '=') {
                advance();
                addToken(TokenType::GREATER_EQUAL, ">=");
            } else {
                addToken(TokenType::GREATER, ">");
            }
            atLineStart = false;
            break;

        case '!':
            if (peek() == '=') {
                advance();
                addToken(TokenType::BANG_EQUAL, "!=");
            }
            atLineStart = false;
            break;

        case '(':
            addToken(TokenType::LEFT_PAREN, "(");
            atLineStart = false;
            break;

        case ')':
            addToken(TokenType::RIGHT_PAREN, ")");
            atLineStart = false;
            break;

        case '{':
            addToken(TokenType::LEFT_BRACE, "{");
            atLineStart = false;
            break;

        case '}':
            addToken(TokenType::RIGHT_BRACE, "}");
            atLineStart = false;
            break;

        case '[':
            addToken(TokenType::LEFT_BRACKET, "[");
            atLineStart = false;
            break;

        case ']':
            addToken(TokenType::RIGHT_BRACKET, "]");
            atLineStart = false;
            break;

        case ',':
            addToken(TokenType::COMMA, ",");
            atLineStart = false;
            break;

        case '.':
            addToken(TokenType::DOT, ".");
            atLineStart = false;
            break;

        case ':':
            if (peek() == ':') {
                advance();
                addToken(TokenType::DOUBLE_COLON, "::");
            } else {
                addToken(TokenType::COLON, ":");
            }
            atLineStart = false;
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
            atLineStart = false;
            break;

        case '"':
            scanString();
            atLineStart = false;
            break;

        case '#':
            scanComment();
            break;

        case '@':
            if (peek() == '"') {
                advance();
                scanInterpolatedString();
            }
            atLineStart = false;
            break;

        default:
            if (isDigit(c)) {
                current--;
                column--;
                scanNumber();
                atLineStart = false;
            } else if (isAlpha(c)) {
                current--;
                column--;
                scanIdentifier();
                atLineStart = false;
            } else {
                addToken(TokenType::UNKNOWN, std::string(1, c));
                atLineStart = false;
            }
            break;
    }
}

void Lexer::scanNumber() {
    size_t start = current;

    while (isDigit(peek())) {
        advance();
    }

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

    advance();

    string str = source.substr(start, current - start - 1);
    addToken(TokenType::STRING_LITERAL, str);
}

void Lexer::scanIdentifier() {
    size_t start = current;

    while (isAlphaNumeric(peek())) {
        advance();
    }

    string text = source.substr(start, current - start);

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

void Lexer::scanInterpolatedString() {
    size_t start = current;
    string result;

    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '{') {
            advance();

            size_t varStart = current;
            while (isAlphaNumeric(peek()) && peek() != '}') {
                advance();
            }

            if (peek() != '}') {
                SourceLocation location(line, column, current - start);
                reporter.error(location, "Expected '}' in interpolated string");
                throw runtime_error("Interpolation error");
            }

            string varName = source.substr(varStart, current - varStart);
            result += "{" + varName + "}";

            advance();
        } else {
            result += peek();
            advance();
        }
    }

    if (isAtEnd()) {
        SourceLocation location(line, column, current - start);
        reporter.error(location, "Unterminated interpolated string");
        throw runtime_error("Unterminated interpolated string");
    }

    advance();

    addToken(TokenType::INTERPOLATED_STRING, result);
}

void Lexer::handleIndentation() {
    if (!atLineStart) return;

    int spaces = 0;

    while (peek() == ' ' || peek() == '\t') {
        if (peek() == '\t') {
            spaces += 4;
        } else {
            spaces += 1;
        }
        advance();
    }

    if (peek() == '\n' || peek() == '#') {
        return;
    }

    currentIndent = spaces;
    atLineStart = false;

    int previousIndent = indentStack.back();

    if (currentIndent > previousIndent) {
        indentStack.push_back(currentIndent);
        addToken(TokenType::INDENT, "INDENT");
    } else if (currentIndent < previousIndent) {
        while (!indentStack.empty() && indentStack.back() > currentIndent) {
            indentStack.pop_back();
            addToken(TokenType::DEDENT, "DEDENT");
        }

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
// LLVM Code Generator (VEREINFACHT!)
// ======================

class CodeGenerator {
public:
    CodeGenerator()
        : context(make_unique<LLVMContext>()),
          builder(make_unique<IRBuilder<>>(*context)),
          module(make_unique<Module>("mylang", *context)) {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        InitializeNativeTargetAsmParser();
    }

    void declarePrintf() {
        FunctionType* printfType = FunctionType::get(
            llvm::Type::getInt32Ty(*context),
            {PointerType::get(llvm::Type::getInt8Ty(*context), 0)},
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
        vector<llvm::Type*> paramTypes;
        for (const auto& param : funcDecl->parameters) {
            paramTypes.push_back(param.resolvedType->toLLVMType(*context));
        }

        llvm::Type* returnType;
        if (funcDecl->name == "main") {
            returnType = llvm::Type::getInt32Ty(*context);
        } else {
            returnType = funcDecl->resolvedReturnType->toLLVMType(*context);
        }
        FunctionType* funcType = FunctionType::get(returnType, paramTypes, false);

        string mangledName = mangleFunctionName(funcDecl->name, funcDecl->parameters);

        if (funcDecl->name != "main") {
            mangledName = "c_" + mangledName;
        }

        Function* function = Function::Create(
            funcType,
            Function::ExternalLinkage,
            mangledName,
            module.get()
        );

        string tableName = mangleFunctionName(funcDecl->name, funcDecl->parameters);
        functions[tableName] = function;
    }

    void declareStringFunctions() {
        FunctionType* strlenType = FunctionType::get(
            llvm::Type::getInt64Ty(*context),
            {PointerType::get(llvm::Type::getInt8Ty(*context), 0)},
            false
        );
        Function::Create(strlenType, Function::ExternalLinkage, "strlen", module.get());

        FunctionType* mallocType = FunctionType::get(
            PointerType::get(llvm::Type::getInt8Ty(*context), 0),
            {llvm::Type::getInt64Ty(*context)},
            false
        );
        Function::Create(mallocType, Function::ExternalLinkage, "malloc", module.get());

        FunctionType* strcpyType = FunctionType::get(
            PointerType::get(llvm::Type::getInt8Ty(*context), 0),
            {
                PointerType::get(llvm::Type::getInt8Ty(*context), 0),
                PointerType::get(llvm::Type::getInt8Ty(*context), 0)
            },
            false
        );
        Function::Create(strcpyType, Function::ExternalLinkage, "strcpy", module.get());

        FunctionType* strcatType = FunctionType::get(
            PointerType::get(llvm::Type::getInt8Ty(*context), 0),
            {
                PointerType::get(llvm::Type::getInt8Ty(*context), 0),
                PointerType::get(llvm::Type::getInt8Ty(*context), 0)
            },
            false
        );
        Function::Create(strcatType, Function::ExternalLinkage, "strcat", module.get());

        FunctionType* sprintfType = FunctionType::get(
            llvm::Type::getInt32Ty(*context),
            {
                PointerType::get(llvm::Type::getInt8Ty(*context), 0),
                PointerType::get(llvm::Type::getInt8Ty(*context), 0)
            },
            true
        );
        Function::Create(sprintfType, Function::ExternalLinkage, "sprintf", module.get());
    }

    void generate(const Program& program) {
        declarePrintf();
        declareStringFunctions();

        for (const auto& stmt : program.statements) {
            if (auto* funcDecl = dynamic_cast<FunctionDeclaration*>(stmt.get())) {
                declareFunction(funcDecl);
            }
        }

        for (const auto& stmt : program.statements) {
            generateStatement(stmt.get());
        }
    }

    void printIR() {
        module->print(outs(), nullptr);
    }

    void writeObjectFile(const string& filename) {
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

        error_code EC;
        raw_fd_ostream dest(filename, EC, sys::fs::OF_None);

        if (EC) {
            errs() << "Could not open file: " << EC.message() << "\n";
            return;
        }

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

    unordered_map<string, Value *> namedValues;
    unordered_map<string, Function *> functions;
    Function *currentFunction = nullptr;

    string mangleFunctionName(const string& name, const vector<Parameter>& parameters) {
        if (name == "main") {
            return "main";
        }

        string mangledName = name;

        for (const auto& parameter : parameters) {
            string typeName = parameter.typeStr;
            // Normalize: Entferne Spaces, ersetze ? und []
            typeName.erase(remove(typeName.begin(), typeName.end(), ' '), typeName.end());
            for (char& c : typeName) {
                if (c == '?' || c == '[' || c == ']') {
                    c = '_';
                }
            }
            mangledName += "_" + typeName;
        }

        if (parameters.empty()) {
            mangledName += "_void";
        }

        return mangledName;
    }

    string mangleFunctionName(const string& name, const vector<shared_ptr<MyType>>& paramTypes) {
        if (name == "main") {
            return "main";
        }

        string mangledName = name;

        for (const auto& type : paramTypes) {
            string typeName = type->toString();
            // WICHTIG: GLEICHE Normalisierung wie oben!
            typeName.erase(remove(typeName.begin(), typeName.end(), ' '), typeName.end());
            for (char& c : typeName) {
                if (c == '?' || c == '[' || c == ']') {
                    c = '_';
                }
            }
            mangledName += "_" + typeName;
        }

        if (paramTypes.empty()) {
            mangledName += "_void";
        }

        return mangledName;
    }

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

    void generateFunction(FunctionDeclaration* funcDecl) {
        string mangledName = mangleFunctionName(funcDecl->name, funcDecl->parameters);
        Function* function = functions[mangledName];
        currentFunction = function;

        BasicBlock* entryBlock = BasicBlock::Create(*context, "entry", function);
        builder->SetInsertPoint(entryBlock);

        for (auto& arg : function->args()) {
            arg.setName(funcDecl->parameters[arg.getArgNo()].name);

            AllocaInst* alloca = builder->CreateAlloca(
                arg.getType(),
                nullptr,
                arg.getName()
            );

            builder->CreateStore(&arg, alloca);

            namedValues[std::string(arg.getName())] = alloca;
        }

        for (const auto& stmt : funcDecl->body) {
            generateStatement(stmt.get());
        }

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

        verifyFunction(*function, &errs());

        namedValues.clear();
        currentFunction = nullptr;
    }

    void generateVariableDeclaration(VariableDeclaration* varDecl) {
        Function* function = builder->GetInsertBlock()->getParent();
        IRBuilder<> tmpBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());

        // VEREINFACHT: Type ist bereits resolved!
        llvm::Type* type = varDecl->resolvedType->toLLVMType(*context);
        Value* initValue = nullptr;

        if (varDecl->initializer) {
            initValue = generateExpression(varDecl->initializer.get());

            // Optional boxing
            if (varDecl->resolvedType->kind == TypeKind::Optional &&
                varDecl->initializer->exprType->kind != TypeKind::Optional &&
                varDecl->initializer->exprType->kind != TypeKind::Null) {

                Function* mallocFunc = module->getFunction("malloc");
                Value* size = ConstantExpr::getSizeOf(initValue->getType());
                Value* ptr = builder->CreateCall(mallocFunc, {size}, "boxed");
                Value* typedPtr = builder->CreateBitCast(ptr, type);
                builder->CreateStore(initValue, typedPtr);
                initValue = typedPtr;
            }
        } else {
            // Default values
            if (varDecl->resolvedType->kind == TypeKind::Int) {
                initValue = ConstantInt::get(*context, APInt(32, 0));
            } else if (varDecl->resolvedType->kind == TypeKind::Bool) {
                initValue = ConstantInt::get(*context, APInt(1, 0));
            } else if (varDecl->resolvedType->kind == TypeKind::Optional ||
                       varDecl->resolvedType->kind == TypeKind::String) {
                initValue = ConstantPointerNull::get(cast<PointerType>(type));
            }
        }

        AllocaInst* alloca = tmpBuilder.CreateAlloca(type, nullptr, varDecl->name);

        if (initValue) {
            builder->CreateStore(initValue, alloca);
        }

        namedValues[varDecl->name] = alloca;
    }

    void generateReturn(ReturnStatement* returnStmt) {
        if (returnStmt->value) {
            Value* retValue = generateExpression(returnStmt->value.get());
            builder->CreateRet(retValue);
        } else {
            builder->CreateRetVoid();
        }
    }

    void generateIf(IfStatement *ifStmt) {
        Value *condition = generateExpression(ifStmt->condition.get());

        if (!condition->getType()->isIntegerTy(1)) {
            condition = builder->CreateICmpNE(
                condition,
                ConstantInt::get(*context, APInt(32, 0)),
                "ifcond"
            );
        }

        Function *function = builder->GetInsertBlock()->getParent();

        BasicBlock *thenBB = BasicBlock::Create(*context, "then", function);
        BasicBlock *elseBB = nullptr;
        BasicBlock *mergeBB = nullptr;

        if (!ifStmt->elseBranch.empty()) {
            elseBB = BasicBlock::Create(*context, "else");
            builder->CreateCondBr(condition, thenBB, elseBB);
        } else {
            mergeBB = BasicBlock::Create(*context, "ifcont");
            builder->CreateCondBr(condition, thenBB, mergeBB);
        }

        builder->SetInsertPoint(thenBB);
        for (const auto &stmt: ifStmt->thenBranch) {
            generateStatement(stmt.get());
        }
        bool thenHasTerminator = builder->GetInsertBlock()->getTerminator() != nullptr;

        bool elseHasTerminator = false;
        if (!ifStmt->elseBranch.empty()) {
            function->insert(function->end(), elseBB);
            builder->SetInsertPoint(elseBB);
            for (const auto &stmt: ifStmt->elseBranch) {
                generateStatement(stmt.get());
            }
            elseHasTerminator = builder->GetInsertBlock()->getTerminator() != nullptr;
        }

        if (!ifStmt->elseBranch.empty()) {
            if (!thenHasTerminator || !elseHasTerminator) {
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
            if (!thenHasTerminator) {
                builder->SetInsertPoint(thenBB);
                builder->CreateBr(mergeBB);
            }
            function->insert(function->end(), mergeBB);
            builder->SetInsertPoint(mergeBB);
        }
    }

    void generateWhile(WhileStatement* whileStmt) {
        Function* function = builder->GetInsertBlock()->getParent();

        BasicBlock* condBB = BasicBlock::Create(*context, "whilecond", function);
        BasicBlock* loopBB = BasicBlock::Create(*context, "whileloop");
        BasicBlock* afterBB = BasicBlock::Create(*context, "afterloop");

        builder->CreateBr(condBB);

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

        function->insert(function->end(), loopBB);
        builder->SetInsertPoint(loopBB);
        for (const auto& stmt : whileStmt->body) {
            generateStatement(stmt.get());
        }
        builder->CreateBr(condBB);

        function->insert(function->end(), afterBB);
        builder->SetInsertPoint(afterBB);
    }

    // VEREINFACHT: Nutzt exprType direkt!
    Value* generateExpression(Expression* expr) {
        if (!expr->exprType) {
            errs() << "Expression has no type!\n";
            return nullptr;
        }

        if (auto* intLit = dynamic_cast<IntLiteral*>(expr)) {
            return ConstantInt::get(*context, APInt(32, intLit->value));
        }
        else if (auto* boolLit = dynamic_cast<BoolLiteral*>(expr)) {
            return ConstantInt::get(*context, APInt(1, boolLit->value ? 1 : 0));
        }
        else if (auto* strLit = dynamic_cast<MyStringLiteral*>(expr)) {
            return builder->CreateGlobalStringPtr(strLit->value);
        }
        else if (dynamic_cast<NullLiteral*>(expr)) {
            return ConstantPointerNull::get(PointerType::getUnqual(*context));
        }
        else if (auto* interpStr = dynamic_cast<InterpolatedString*>(expr)) {
            return generateInterpolatedString(interpStr);
        }
        else if (auto* var = dynamic_cast<Variable*>(expr)) {
            Value* varPtr = namedValues[var->name];
            if (!varPtr) {
                errs() << "Unknown variable: " << var->name << "\n";
                return nullptr;
            }

            if (auto* allocaInst = dyn_cast<AllocaInst>(varPtr)) {
                return builder->CreateLoad(
                    allocaInst->getAllocatedType(),
                    varPtr,
                    var->name.c_str()
                );
            } else {
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

        left = convertToString(left, binOp->left->exprType);
        right = convertToString(right, binOp->right->exprType);

        if (!left || !right) return nullptr;

        Function *strlenFunc = module->getFunction("strlen");
        Function *mallocFunc = module->getFunction("malloc");
        Function *strcpyFunc = module->getFunction("strcpy");
        Function *strcatFunc = module->getFunction("strcat");

        Value *len1 = builder->CreateCall(strlenFunc, {left}, "len1");
        Value *len2 = builder->CreateCall(strlenFunc, {right}, "len2");
        Value *totalLen = builder->CreateAdd(len1, len2, "totallen");
        totalLen = builder->CreateAdd(
            totalLen,
            ConstantInt::get(*context, APInt(64, 1)),
            "totallen_plus1"
        );

        Value *result = builder->CreateCall(mallocFunc, {totalLen}, "concat_result");

        builder->CreateCall(strcpyFunc, {result, left});
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

        if (binOp->op == TokenType::EQUAL_EQUAL) {
            if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
                return builder->CreateICmpEQ(left, right, "ptreqtmp");
            }
            return builder->CreateICmpEQ(left, right, "eqtmp");
        }

        if (binOp->op == TokenType::BANG_EQUAL) {
            if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
                return builder->CreateICmpNE(left, right, "ptrnetmp");
            }
            return builder->CreateICmpNE(left, right, "netmp");
        }

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

    Value* generateInterpolatedString(InterpolatedString* interpStr) {
        string template_str = interpStr->template_str;
        vector<Value*> parts;

        size_t lastPos = 0;
        size_t pos = 0;

        while ((pos = template_str.find('{', lastPos)) != string::npos) {
            if (pos > lastPos) {
                string literal = template_str.substr(lastPos, pos - lastPos);
                parts.push_back(builder->CreateGlobalStringPtr(literal));
            }

            size_t end = template_str.find('}', pos);
            string varName = template_str.substr(pos + 1, end - pos - 1);

            Value* varPtr = namedValues[varName];
            if (!varPtr) {
                errs() << "Unknown variable in interpolation: " << varName << "\n";
                return nullptr;
            }

            Value* varValue = nullptr;
            if (auto* allocaInst = dyn_cast<AllocaInst>(varPtr)) {
                varValue = builder->CreateLoad(
                    allocaInst->getAllocatedType(),
                    varPtr,
                    varName.c_str()
                );
            }

            // Finde Type aus Symbol Table (müsste eigentlich auch in Expression gespeichert sein)
            shared_ptr<MyType> varType = MyType::Int(); // Default
            for (const auto& [name, type] : namedValues) {
                if (name == varName) {
                    if (auto* ai = dyn_cast<AllocaInst>(varPtr)) {
                        llvm::Type* llvmType = ai->getAllocatedType();
                        if (llvmType->isIntegerTy(32)) varType = MyType::Int();
                        else if (llvmType->isIntegerTy(1)) varType = MyType::Bool();
                        else if (llvmType->isPointerTy()) varType = MyType::String();
                    }
                    break;
                }
            }

            Value* strValue = convertToString(varValue, varType);
            parts.push_back(strValue);

            lastPos = end + 1;
        }

        if (lastPos < template_str.length()) {
            string literal = template_str.substr(lastPos);
            parts.push_back(builder->CreateGlobalStringPtr(literal));
        }

        if (parts.empty()) {
            return builder->CreateGlobalStringPtr("");
        }

        Value* result = parts[0];
        for (size_t i = 1; i < parts.size(); i++) {
            Function *strlenFunc = module->getFunction("strlen");
            Function *mallocFunc = module->getFunction("malloc");
            Function *strcpyFunc = module->getFunction("strcpy");
            Function *strcatFunc = module->getFunction("strcat");

            Value *len1 = builder->CreateCall(strlenFunc, {result});
            Value *len2 = builder->CreateCall(strlenFunc, {parts[i]});
            Value *totalLen = builder->CreateAdd(len1, len2);
            totalLen = builder->CreateAdd(totalLen, ConstantInt::get(*context, APInt(64, 1)));

            Value *newResult = builder->CreateCall(mallocFunc, {totalLen});
            builder->CreateCall(strcpyFunc, {newResult, result});
            builder->CreateCall(strcatFunc, {newResult, parts[i]});

            result = newResult;
        }

        return result;
    }

    Value* generateUnaryOp(UnaryOperation* unaryOp) {
        Value* operand = generateExpression(unaryOp->operand.get());
        if (!operand) return nullptr;

        switch (unaryOp->op) {
            case TokenType::MINUS:
                if (operand->getType()->isIntegerTy()) {
                    return builder->CreateNeg(operand, "negtmp");
                } else if (operand->getType()->isDoubleTy()) {
                    return builder->CreateFNeg(operand, "negtmp");
                }
                break;
            case TokenType::NOT:
                return builder->CreateNot(operand, "nottmp");
            default:
                errs() << "Unknown unary operator\n";
                return nullptr;
        }
        return nullptr;
    }

    Value *generateLogicalOp(BinaryOperation *binOp) {
        Function *function = builder->GetInsertBlock()->getParent();

        Value *left = generateExpression(binOp->left.get());
        if (!left) return nullptr;

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
            builder->CreateCondBr(left, rightBB, mergeBB);
        } else {
            builder->CreateCondBr(left, mergeBB, rightBB);
        }

        function->insert(function->end(), rightBB);
        builder->SetInsertPoint(rightBB);
        Value *right = generateExpression(binOp->right.get());
        if (!right) return nullptr;

        if (!right->getType()->isIntegerTy(1)) {
            right = builder->CreateICmpNE(
                right,
                ConstantInt::get(*context, APInt(32, 0)),
                "tobool"
            );
        }

        BasicBlock *rightEndBB = builder->GetInsertBlock();
        builder->CreateBr(mergeBB);

        function->insert(function->end(), mergeBB);
        builder->SetInsertPoint(mergeBB);

        PHINode *phi = builder->CreatePHI(llvm::Type::getInt1Ty(*context), 2, "logical_result");

        if (binOp->op == TokenType::AND) {
            phi->addIncoming(ConstantInt::getFalse(*context), startBB);
            phi->addIncoming(right, rightEndBB);
        } else {
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

        Value* arg = generateExpression(call->arguments[0].get());
        if (!arg) return nullptr;

        shared_ptr<MyType> argType = call->arguments[0]->exprType;

        Value* formatStr = nullptr;
        vector<Value*> printfArgs;

        // Handle optional types: unwrap or print "null"
        if (argType->kind == TypeKind::Optional) {
            // Check if null
            Value* isNull = builder->CreateICmpEQ(arg, ConstantPointerNull::get(cast<PointerType>(arg->getType())));

            Function* function = builder->GetInsertBlock()->getParent();
            BasicBlock* nullBB = BasicBlock::Create(*context, "print_null");
            BasicBlock* valueBB = BasicBlock::Create(*context, "print_value");
            BasicBlock* contBB = BasicBlock::Create(*context, "print_cont");

            builder->CreateCondBr(isNull, nullBB, valueBB);

            // Null branch: print "null"
            function->insert(function->end(), nullBB);
            builder->SetInsertPoint(nullBB);
            Value* nullStr = builder->CreateGlobalStringPtr("null\n");
            builder->CreateCall(printfFunc, {nullStr});
            builder->CreateBr(contBB);

            // Value branch: unwrap and print
            function->insert(function->end(), valueBB);
            builder->SetInsertPoint(valueBB);

            // Unwrap: load from pointer
            Value* unwrapped = builder->CreateLoad(
                argType->elementType->toLLVMType(*context),
                arg,
                "unwrapped"
            );

            // Print based on inner type
            if (argType->elementType->kind == TypeKind::Int) {
                formatStr = builder->CreateGlobalStringPtr("%d\n");
                builder->CreateCall(printfFunc, {formatStr, unwrapped});
            } else if (argType->elementType->kind == TypeKind::Bool) {
                Value* boolAsInt = builder->CreateZExt(unwrapped, llvm::Type::getInt32Ty(*context));
                Value* trueStr = builder->CreateGlobalStringPtr("true");
                Value* falseStr = builder->CreateGlobalStringPtr("false");
                Value* isTrue = builder->CreateICmpNE(boolAsInt, ConstantInt::get(*context, APInt(32, 0)));
                Value* selectedStr = builder->CreateSelect(isTrue, trueStr, falseStr);
                formatStr = builder->CreateGlobalStringPtr("%s\n");
                builder->CreateCall(printfFunc, {formatStr, selectedStr});
            } else if (argType->elementType->kind == TypeKind::String) {
                // String is already a pointer, load gives us the string pointer
                formatStr = builder->CreateGlobalStringPtr("%s\n");
                builder->CreateCall(printfFunc, {formatStr, unwrapped});
            }

            builder->CreateBr(contBB);

            // Continue
            function->insert(function->end(), contBB);
            builder->SetInsertPoint(contBB);

            return nullptr;
        }

        // Non-optional types (original code)
        if (argType->kind == TypeKind::Int) {
            formatStr = builder->CreateGlobalStringPtr("%d\n");
            printfArgs.push_back(formatStr);
            printfArgs.push_back(arg);
        } else if (argType->kind == TypeKind::Bool) {
            Value* boolAsInt = builder->CreateZExt(arg, llvm::Type::getInt32Ty(*context), "booltoint");
            Value* trueStr = builder->CreateGlobalStringPtr("true");
            Value* falseStr = builder->CreateGlobalStringPtr("false");

            Value* isTrue = builder->CreateICmpNE(
                boolAsInt,
                ConstantInt::get(*context, APInt(32, 0))
            );
            Value* selectedStr = builder->CreateSelect(isTrue, trueStr, falseStr);

            formatStr = builder->CreateGlobalStringPtr("%s\n");
            printfArgs.push_back(formatStr);
            printfArgs.push_back(selectedStr);
        } else if (argType->kind == TypeKind::Float) {
            formatStr = builder->CreateGlobalStringPtr("%f\n");
            printfArgs.push_back(formatStr);
            printfArgs.push_back(arg);
        } else if (argType->kind == TypeKind::String) {
            formatStr = builder->CreateGlobalStringPtr("%s\n");
            printfArgs.push_back(formatStr);
            printfArgs.push_back(arg);
        } else {
            errs() << "Unsupported type for print(): " << argType->toString() << "\n";
            return nullptr;
        }

        return builder->CreateCall(printfFunc, printfArgs, "printcall");
    }

    Value* generateFunctionCall(FunctionCall* call) {
        if (call->name == "print") {
            return generatePrint(call);
        }

        // Generiere argument types
        vector<shared_ptr<MyType>> argTypes;
        for (const auto& arg : call->arguments) {
            argTypes.push_back(arg->exprType);
        }

        string mangledName = mangleFunctionName(call->name, argTypes);

        Function* calleeF = functions[mangledName];
        if (!calleeF) {
            errs() << "Unknown function: " << call->name
                   << " (mangled: " << mangledName << ")\n";
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

        if (calleeF->getReturnType()->isVoidTy()) {
            builder->CreateCall(calleeF, args);
            return nullptr;
        }

        return builder->CreateCall(calleeF, args, "calltmp");
    }

    // VEREINFACHT: mit Type parameter!
    Value* convertToString(Value* val, shared_ptr<MyType> type) {
        if (!val || !type) return nullptr;

        if (type->kind == TypeKind::String) {
            return val;
        }

        Function* mallocFunc = module->getFunction("malloc");
        Function* sprintfFunc = module->getFunction("sprintf");

        Value* buffer = builder->CreateCall(
            mallocFunc,
            {ConstantInt::get(*context, APInt(64, 32))},
            "str_buffer"
        );

        if (type->kind == TypeKind::Int) {
            Value* format = builder->CreateGlobalStringPtr("%d");
            builder->CreateCall(sprintfFunc, {buffer, format, val});
            return buffer;
        }
        else if (type->kind == TypeKind::Bool) {
            Value* boolAsInt = builder->CreateZExt(val, llvm::Type::getInt32Ty(*context));
            Value* trueStr = builder->CreateGlobalStringPtr("true");
            Value* falseStr = builder->CreateGlobalStringPtr("false");
            Value* isTrue = builder->CreateICmpNE(
                boolAsInt,
                ConstantInt::get(*context, APInt(32, 0))
            );
            return builder->CreateSelect(isTrue, trueStr, falseStr);
        }
        else if (type->kind == TypeKind::Float) {
            Value* format = builder->CreateGlobalStringPtr("%.2f");
            builder->CreateCall(sprintfFunc, {buffer, format, val});
            return buffer;
        }

        errs() << "Cannot convert type to string: " << type->toString() << "\n";
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

        if (debug) {
            cout << "=== TOKENS ===" << endl;
            for (const auto& token : tokens) {
                cout << token << endl;
            }
            cout << endl;
        }

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

        // NEU: Type Checking Pass!
        TypeChecker typeChecker(reporter);
        typeChecker.check(program.get());

        if (reporter.hasError()) {
            return 1;
        }

        if (debug) {
            cout << "=== LLVM IR ===" << endl;
        }

        CodeGenerator codegen;
        codegen.generate(*program);

        if (debug) {
            codegen.printIR();
            cout << endl;
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