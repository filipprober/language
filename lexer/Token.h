#pragma once

using namespace std;

enum class TokenType
{
    NAMESPACE, USE,
    NEW, CLASS, ABSTRACT, INTERFACE, TRAIT, ENUM,
    EXTENDS, IMPLEMENTS,
    PUBLIC, PRIVATE, PROTECTED,
    FN, VAR, CONST, INIT, THIS, SUPER, END,
    IF, ELSE, WHILE, FOR, MATCH, CASE, DEFAULT,
    RETURN, BREAK, CONTINUE, WHEN,
    ASYNC, AWAIT,
    TRY, CATCH, FINALLY, THROW,

    INT, FLOAT, STRING, BOOL, VOID, ANY, QUESTION,

    TRUE, FALSE, NONE,
    IDENTIFIER,
    INTEGER_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    INTERPOLATED_STRING,

    PLUS, MINUS, STAR, SLASH, PERCENT,
    EQUAL, EQUAL_EQUAL, BANG_EQUAL,
    LESS, LESS_EQUAL, GREATER, GREATER_EQUAL,
    PLUS_EQUAL, MINUS_EQUAL, STAR_EQUAL, SLASH_EQUAL,

    AND, OR, NOT, IS, AS, IN,

    LEFT_PAREN, RIGHT_PAREN,
    LEFT_BRACE, RIGHT_BRACE,
    LEFT_BRACKET, RIGHT_BRACKET,
    COMMA, DOT, COLON, SEMICOLON,
    ARROW, FAT_ARROW, DOUBLE_COLON,

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
    }

    Token(TokenType type, const string& lexeme, SourceLocation location) :
        type(type),
        lexeme(lexeme),
        location(location)
    {
    }
};
