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

        if (match(TokenType::CLASS)) {
            return parseClassDeclaration();
        }

        if (match(TokenType::INTERFACE)) {
            return parseInterfaceDeclaration();
        }

        if (match(TokenType::TRAIT)) {
            return parseTraitDeclaration();
        }

        if (match(TokenType::ABSTRACT)) {
            if (match(TokenType::CLASS)) {
                return parseClassDeclaration(true); // <- isAbstract = true
            }
        }

        return parseExpressionStatement();
    }

    unique_ptr<Statement> parseClassDeclaration(bool isAbstract = false) {
        Token className = consume(TokenType::IDENTIFIER, "Expected class naem");

        // extends
        string baseClass;
        if (match(TokenType::EXTENDS)) {
            Token base = consume(TokenType::IDENTIFIER, "Expected base class name");
            baseClass = base.lexeme;
        }

        auto classDecl = make_unique<ClassDeclaration>(className.lexeme, baseClass, isAbstract);

        // implements
        if (match(TokenType::IMPLEMENTS)) {
            do {
                Token iface = consume(TokenType::IDENTIFIER, "Expected interface name");
                classDecl->interfaces.push_back(iface.lexeme);
            } while (match(TokenType::COMMA));
        }

        consume(TokenType::COLON, "Expected ':' after class signature");
        while (match(TokenType::NEWLINE)) {}
        consume(TokenType::INDENT, "Expected indentation");

        // Parse class body
        while (!check(TokenType::DEDENT) && !isAtEnd()) {
            while (match(TokenType::NEWLINE)) {}

            if (check(TokenType::DEDENT)) break;

            // Visibility
            Visibility vis = Visibility::Private;
            if (match(TokenType::PUBLIC)) {
                vis = Visibility::Public;
            } else if (match(TokenType::PROTECTED)) {
                vis = Visibility::Protected;
            } else if (match(TokenType::PRIVATE)) {
                vis = Visibility::Private;
            }

            // Property oder Method?
            if (match(TokenType::VAR)) {
                // Property
                Token propName = consume(TokenType::IDENTIFIER, "Expected property name");
                consume(TokenType::COLON, "Expected ':' after property name");
                string propType = parseType();

                unique_ptr<Expression> initializer = nullptr;
                if (match(TokenType::EQUAL)) {
                    initializer = parseExpression();
                }

                classDecl->properties.emplace_back(propName.lexeme, propType,
                                                   std::move(initializer), vis);
                match(TokenType::NEWLINE);
            } else if (match(TokenType::INIT)) {
                // Constructor
                auto ctor = parseConstructor(vis);
                classDecl->constructors.push_back(std::move(ctor));
            } else if (match(TokenType::CONST)) {
                // Static method
                if (!match(TokenType::FN)) {
                    reportError("Expected 'fn' after 'const'");
                }
                auto method = parseMethod(vis, true);
                classDecl->methods.push_back(std::move(method));
            } else if (match(TokenType::ABSTRACT)) {
                // Abstract method
                if (!match(TokenType::FN)) {
                    reportError("Expected 'fn' after 'abstract'");
                }
                auto method = parseMethod(vis, false, true);
                classDecl->methods.push_back(std::move(method));
            } else if (match(TokenType::FN)) {
                // Regular method
                auto method = parseMethod(vis);
                classDecl->methods.push_back(std::move(method));
            } else {
                reportError("Unexpected token in class body");
            }
        }

        consume(TokenType::DEDENT, "Expected dedent after class body");

        return classDecl;
    }

    unique_ptr<ConstructorDeclaration> parseConstructor(Visibility vis) {
        consume(TokenType::LEFT_PAREN, "Expected '(' after init");

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
        consume(TokenType::COLON, "Expected ':' after constructor signature");

        while (match(TokenType::NEWLINE)) {
        }
        vector<unique_ptr<Statement>> body = parseBlock();

        auto ctor = make_unique<ConstructorDeclaration>(std::move(parameters),
                                                        std::move(body), vis);

        // Check für super() call im body
        // TODO: Parse super() als erstes Statement

        return ctor;
    }

    unique_ptr<MethodDeclaration> parseMethod(Visibility vis, bool isStatic = false,
                                              bool isAbstract = false) {
        Token methodName = consume(TokenType::IDENTIFIER, "Expected method name");
        consume(TokenType::LEFT_PAREN, "Expected '(' after method name");

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

        consume(TokenType::COLON, "Expected ':' after method signature");

        vector<unique_ptr<Statement> > body;
        if (!isAbstract) {
            while (match(TokenType::NEWLINE)) {
            }
            body = parseBlock();
        } else {
            match(TokenType::NEWLINE);
        }

        return make_unique<MethodDeclaration>(methodName.lexeme, std::move(parameters),
                                              returnType, std::move(body), vis,
                                              isStatic, isAbstract);
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

        return parsePostfix();
    }

    unique_ptr<Expression> parsePostfix() {
        auto expr = parsePrimary();

        while (true) {
            if (check(TokenType::DOT)) {
                // Schaue voraus: ist das nächste Token ein Identifier?
                // Wenn ja -> Member Access, wenn nein -> kein DOT konsumieren
                if (current + 1 < tokens.size() &&
                    tokens[current + 1].type == TokenType::IDENTIFIER) {

                    match(TokenType::DOT);  // Jetzt konsumieren
                    Token memberName = consume(TokenType::IDENTIFIER, "Expected member name after '.'");

                    if (match(TokenType::LEFT_PAREN)) {
                        // Method call
                        vector<unique_ptr<Expression>> arguments;
                        if (!check(TokenType::RIGHT_PAREN)) {
                            do {
                                arguments.push_back(parseExpression());
                            } while (match(TokenType::COMMA));
                        }
                        consume(TokenType::RIGHT_PAREN, "Expected ')' after arguments");

                        expr = make_unique<MethodCall>(std::move(expr), memberName.lexeme,
                                                      std::move(arguments));
                    } else {
                        // Member access
                        expr = make_unique<MemberAccess>(std::move(expr), memberName.lexeme);
                    }
                    } else {
                        // Kein Identifier nach DOT -> kein Member Access
                        // Lasse DOT für String-Concatenation
                        break;
                    }
            } else {
                break;
            }
        }

        return expr;
    }

    unique_ptr<Statement> parseInterfaceDeclaration() {
        Token interfaceName = consume(TokenType::IDENTIFIER, "Expected interface name");
        auto interfaceDecl = make_unique<InterfaceDeclaration>(interfaceName.lexeme);

        consume(TokenType::COLON, "Expected ':' after interface name");
        while (match(TokenType::NEWLINE)) {
        }
        consume(TokenType::INDENT, "Expected indentation");

        while (!check(TokenType::DEDENT) && !isAtEnd()) {
            while (match(TokenType::NEWLINE)) {
            }
            if (check(TokenType::DEDENT)) break;

            // Interface methods sind immer public und abstract
            if (!match(TokenType::FN)) {
                reportError("Expected 'fn' in interface");
            }

            auto method = parseMethod(Visibility::Public, false, true);
            interfaceDecl->methods.push_back(std::move(method));
        }

        consume(TokenType::DEDENT, "Expected dedent after interface body");
        return interfaceDecl;
    }

    unique_ptr<Statement> parseTraitDeclaration() {
        Token traitName = consume(TokenType::IDENTIFIER, "Expected trait name");
        auto traitDecl = make_unique<TraitDeclaration>(traitName.lexeme);

        consume(TokenType::COLON, "Expected ':' after trait name");
        while (match(TokenType::NEWLINE)) {
        }
        consume(TokenType::INDENT, "Expected indentation");

        while (!check(TokenType::DEDENT) && !isAtEnd()) {
            while (match(TokenType::NEWLINE)) {
            }
            if (check(TokenType::DEDENT)) break;

            if (match(TokenType::VAR)) {
                // Trait property
                Token propName = consume(TokenType::IDENTIFIER, "Expected property name");
                consume(TokenType::COLON, "Expected ':' after property name");
                string propType = parseType();

                unique_ptr<Expression> initializer = nullptr;
                if (match(TokenType::EQUAL)) {
                    initializer = parseExpression();
                }

                traitDecl->properties.emplace_back(propName.lexeme, propType,
                                                   std::move(initializer), Visibility::Public);
                match(TokenType::NEWLINE);
            } else if (match(TokenType::FN)) {
                auto method = parseMethod(Visibility::Public, false, false);
                traitDecl->methods.push_back(std::move(method));
            } else {
                reportError("Unexpected token in trait body");
            }
        }

        consume(TokenType::DEDENT, "Expected dedent after trait body");
        return traitDecl;
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

        if (match(TokenType::NEW)) {
            Token className = consume(TokenType::IDENTIFIER, "Expected class name after 'new'");
            consume(TokenType::LEFT_PAREN, "Expected '(' after class name");

            vector<unique_ptr<Expression>> arguments;
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    arguments.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }

            consume(TokenType::RIGHT_PAREN, "Expected ')' after arguments");
            return make_unique<NewExpression>(className.lexeme, std::move(arguments));
        }

        if (match(TokenType::THIS)) {
            return make_unique<ThisExpression>();
        }

        if (match(TokenType::SUPER)) {
            if (match(TokenType::LEFT_PAREN)) {
                // super(...) call
                vector<unique_ptr<Expression>> arguments;
                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        arguments.push_back(parseExpression());
                    } while (match(TokenType::COMMA));
                }
                consume(TokenType::RIGHT_PAREN, "Expected ')' after super arguments");
                return make_unique<SuperCall>(std::move(arguments));
            } else {
                return make_unique<SuperExpression>();
            }
        }

        if (match(TokenType::LEFT_BRACKET)) {
            vector<unique_ptr<Expression>> elements;

            if (!check(TokenType::RIGHT_BRACKET)) {
                do {
                    elements.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }

            consume(TokenType::RIGHT_BRACKET, "Expected ']' after array elements");
            auto arrayLit = make_unique<ArrayLiteral>(std::move(elements));

            if (!arrayLit->elements.empty()) {
                // Nimm den Type des ersten Elements als Element-Type
                // (sollte eigentlich vom TypeChecker gemacht werden)
                arrayLit->exprType = MyType::Array(arrayLit->elements[0]->exprType);
            }

            return arrayLit;
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