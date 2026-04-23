#ifndef LEXER_H
#define LEXER_H

typedef enum {
    // literals
    T_INT,
    T_FLOAT,
    T_STRING,
    T_IDENT,

    // operators
    T_EQ,
    T_EQEQ,
    T_NEQ,
    T_LT,
    T_GT,
    T_LTE,
    T_GTE,
    T_PLUS,
    T_MINUS,
    T_STAR,
    T_STARSTAR,       // ** (power)
    T_SLASH,
    T_PERCENT,        // % (modulo)
    T_CARET,          // ^ (power alt)
    T_PLUSPLUS,
    T_MINUSMINUS,
    T_SEMICOLON,
    T_LPAREN,
    T_RPAREN,
    T_LBRACE,
    T_RBRACE,
    T_LBRACKET,       // [
    T_RBRACKET,       // ]
    T_DOT,            // .
    T_COMMA,
    T_BANG,           // !
    T_AND,            // &&
    T_OR,             // ||
    T_DOLLAR_PAREN,   // $(

    // Keywords
    T_IF,
    T_ELSE,
    T_FOR,
    T_WHILE,
    T_DEF,
    T_RETURN,
    T_PRINT,
    T_PACKAGES,
    T_TRUE,
    T_FALSE,
    T_LEN,

    T_EOF
} TokenType;

typedef struct {
    TokenType type;
    char      value[256];
} Token;

int lex(const char *src, Token *out);

#endif