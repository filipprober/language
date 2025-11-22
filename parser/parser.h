#ifndef LANG_PARSER_H
#define LANG_PARSER_H

#include "../ast/AstNode.h"
#include "../typechecker.h"

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

        if (match(TokenType::TRY)) {
            cout << "using try" << endl;
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

#endif //LANG_PARSER_H