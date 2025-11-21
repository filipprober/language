#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>

using namespace std;


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
    int line;
    int column;

    Token(TokenType type, const string& lexeme, int line, int column) :
        type(type),
        lexeme(lexeme),
        line(line),
        column(column)
    {
        //
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
    explicit Parser(vector<Token> tokens) :
        tokens(std::move(tokens)) {
        //
    }

    unique_ptr<Program> parse() {
        auto program = make_unique<Program>();

        while (!isAtEnd()) {
            try {
                // Skip newlines and indents at top level
                while (match({TokenType::NEWLINE, TokenType::INDENT, TokenType::DEDENT})) {}

                if (isAtEnd()) break;

                auto statement = parseStatement();
                if (statement) {
                    program->statements.push_back(std::move(statement));
                }
            } catch (const exception& e) {
                cerr << "Parse error: " << e.what() << endl;
                synchronize();
            }
        }

        return program;
    }

private:
    vector<Token> tokens;
    size_t current;

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

        throw runtime_error(message + " at line " + to_string(peek().line) + ":" + to_string(peek().column));
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

                Token paramType = advance();
                parameters.emplace_back(paramName.lexeme, paramType.lexeme);
            } while (match(TokenType::COMMA));
        }

        consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters");

        // Parse return type (optional)
        string returnType;
        if (match(TokenType::ARROW)) {
            Token typeToken = advance();
            returnType = typeToken.lexeme;
        }

        consume(TokenType::COLON, "Expected ':' after function signature");
        match(TokenType::NEWLINE);

        // Parse body (indented block)
        vector<unique_ptr<Statement>> body = parseBlock();

        return make_unique<FunctionDeclaration>(
            name.lexeme,
            std::move(parameters),
            returnType,
            std::move(body)
        );
    }

    // Parse If Statement
    unique_ptr<Statement> parseIfStatement() {
        auto condition = parseExpression();
        consume(TokenType::COLON, "Expected ':' after if condition");
        match(TokenType::NEWLINE);

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
        match(TokenType::NEWLINE);

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

        // Skip newlines
        while (match(TokenType::NEWLINE)) {}

        // Parse statements until DEDENT
        while (!check(TokenType::DEDENT) && !isAtEnd()) {
            // Skip empty lines
            while (match(TokenType::NEWLINE)) {}

            if (check(TokenType::DEDENT) || isAtEnd()) {
                break;
            }

            statements.push_back(parseStatement());
        }

        // Expect DEDENT at end of block
        consume(TokenType::DEDENT, "Expected dedent to end block");

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
        return parseComparison();
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
        auto expression = parseMultiplication();

        while (match({TokenType::PLUS, TokenType::MINUS})) {
            TokenType op = previous().type;
            auto right = parseMultiplication();
            expression = make_unique<BinaryOperation>(std::move(expression), op, std::move(right));
        }

        return expression;
    }

    unique_ptr<Expression> parseMultiplication() {
        auto expression = parsePrimary();

        while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
            TokenType op = previous().type;
            auto right = parsePrimary();
            expression = make_unique<BinaryOperation>(std::move(expression), op, std::move(right));
        }

        return expression;
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
            return make_unique<StringLiteral>(previous().lexeme);
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

        throw runtime_error("Expected expression at line" + to_string(peek().line));
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
        << token.line << ":" << token.column << ")";

    return os;
}

class Lexer
{
public:
    explicit Lexer(const string& source);

    vector<Token> tokenize();

private:
    string source;
    size_t current = 0;
    int line = 1;
    int column = 1;
    vector<Token> tokens;

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
};

Lexer::Lexer(const string& source) : source(source) {}

vector<Token> Lexer::tokenize() {
    while (!isAtEnd()) {
        // Handle indentation at line start
        if (atLineStart && peek() != '\n') {
            handleIndentation();
        }

        scanToken();
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

        case '*':
            if (peek() == '*') {
                advance();
                addToken(TokenType::STAR_EQUAL, "*=");
            } else {
                addToken(TokenType::STAR, "*");
            }
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
        throw std::runtime_error("Unterminated string");
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
            throw runtime_error("Indentation error at line " + to_string(line));
        }
    }
    // If currentIndent == previousIndent: same level, do nothing
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

int main(int argc, char* argv[]) {
    bool debug = true;

    if (argc != 2) {
        cerr << "Usage: mylang <file.ml>" << endl;
        return 1;
    }

    ifstream file(argv[1]);
    if (!file) {
        cerr << "Could not open file: " << argv[1] << endl;
        return 1;
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string source = buffer.str();

    // Tokenize
    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    if (debug) {
        cout << "=== TOKENS ===" << endl;
        for (const auto& token : tokens) {
            cout << token << endl;
        }
    }

    Parser parser(tokens);
    auto program = parser.parse();

    if (debug) {
        cout << "=== AST ===" << endl;
        program->print();
    }

    return 0;
}
