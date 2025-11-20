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
        left(move(left)),
        op(op),
        right(move(right)) {
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
        initializer(move(initializer)) {
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
        value(move(value)) {}

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
        expression(move(expression)) {}

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "ExpressionStatement" << endl;
        expression->print(indent + 2);
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
        scanToken();
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
            // Skip whitespace.
            break;

        case '\n':
            addToken(TokenType::NEWLINE, "\\n");

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
    auto left = make_unique<IntLiteral>(5);
    auto right = make_unique<IntLiteral>(10);
    auto addition = make_unique<BinaryOperation>(
        move(left),
        TokenType::PLUS,
        move(right)
    );

    auto varDeclaration = make_unique<VariableDeclaration>(
        "x",            // name
        "int",           // type
        move(addition)
    );

    Program program;
    program.statements.push_back(move(varDeclaration));

    cout << "=== AST ===" << endl;
    program.print();

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

    for (const auto& token : tokens) {
        cout << token << endl;
    }

    return 0;
}
