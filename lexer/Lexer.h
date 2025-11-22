#pragma once

#include "../exceptionreporter.h"

using namespace std;

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
