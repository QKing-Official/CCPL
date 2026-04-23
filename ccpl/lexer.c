#include "lexer.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

static const char *src;
static int pos;

static char peek()       { return src[pos]; }
static char peek2()      { return src[pos] ? src[pos+1] : '\0'; }
static char adv()        { return src[pos++]; }

static void add(Token *t, int *n, TokenType type, const char *v) {
    t[*n].type = type;
    strncpy(t[*n].value, v, sizeof(t[*n].value) - 1);
    t[*n].value[sizeof(t[*n].value) - 1] = '\0';
    (*n)++;
}

static struct { const char *word; TokenType type; } keywords[] = {
    { "if",       T_IF       },
    { "else",     T_ELSE     },
    { "for",      T_FOR      },
    { "while",    T_WHILE    },
    { "def",      T_DEF      },
    { "return",   T_RETURN   },
    { "print",    T_PRINT    },
    { "packages", T_PACKAGES },
    { "true",     T_TRUE     },
    { "false",    T_FALSE    },
    { "len",      T_LEN      },
    { NULL,       T_EOF      }
};

static TokenType keyword_type(const char *w) {
    for (int k = 0; keywords[k].word; k++)
        if (strcmp(keywords[k].word, w) == 0)
            return keywords[k].type;
    return T_IDENT;
}

int lex(const char *input, Token *t) {
    src = input;
    pos = 0;
    int n = 0;

    while (peek()) {

        /* skip whitespace and comments */
        if (isspace((unsigned char)peek())) { adv(); continue; }
        if (peek() == '#') {
            while (peek() && peek() != '\n') adv();
            continue;
        }

        /* $( shell capture */
        if (peek() == '$' && peek2() == '(') {
            adv(); adv(); /* skip $( */
            char buf[256]; int j = 0;
            int depth = 1;
            while (peek() && depth > 0) {
                if (peek() == '(') depth++;
                else if (peek() == ')') { depth--; if (depth == 0) { adv(); break; } }
                else if (peek() == '"' && j < 254) { buf[j++] = '\\'; buf[j++] = adv(); continue; }
                if (j < 255) buf[j++] = adv(); else adv();
            }
            buf[j] = '\0';
            add(t, &n, T_DOLLAR_PAREN, buf);
            continue;
        }

        /* three-char: ... none yet */

        /* two-char operators */
        if (peek() == '=' && peek2() == '=') { adv(); adv(); add(t,&n,T_EQEQ,    "=="); continue; }
        if (peek() == '!' && peek2() == '=') { adv(); adv(); add(t,&n,T_NEQ,     "!="); continue; }
        if (peek() == '<' && peek2() == '=') { adv(); adv(); add(t,&n,T_LTE,     "<="); continue; }
        if (peek() == '>' && peek2() == '=') { adv(); adv(); add(t,&n,T_GTE,     ">="); continue; }
        if (peek() == '+' && peek2() == '+') { adv(); adv(); add(t,&n,T_PLUSPLUS,"++"); continue; }
        if (peek() == '-' && peek2() == '-') { adv(); adv(); add(t,&n,T_MINUSMINUS,"--"); continue; }
        if (peek() == '*' && peek2() == '*') { adv(); adv(); add(t,&n,T_STARSTAR,"**"); continue; }
        if (peek() == '&' && peek2() == '&') { adv(); adv(); add(t,&n,T_AND,     "&&"); continue; }
        if (peek() == '|' && peek2() == '|') { adv(); adv(); add(t,&n,T_OR,      "||"); continue; }

        /* single-char operators */
        if (peek() == '=') { adv(); add(t,&n,T_EQ,       "=");  continue; }
        if (peek() == '<') { adv(); add(t,&n,T_LT,       "<");  continue; }
        if (peek() == '>') { adv(); add(t,&n,T_GT,       ">");  continue; }
        if (peek() == '+') { adv(); add(t,&n,T_PLUS,     "+");  continue; }
        if (peek() == '-') { adv(); add(t,&n,T_MINUS,    "-");  continue; }
        if (peek() == '*') { adv(); add(t,&n,T_STAR,     "*");  continue; }
        if (peek() == '/') { adv(); add(t,&n,T_SLASH,    "/");  continue; }
        if (peek() == '%') { adv(); add(t,&n,T_PERCENT,  "%");  continue; }
        if (peek() == '^') { adv(); add(t,&n,T_CARET,    "^");  continue; }
        if (peek() == '!') { adv(); add(t,&n,T_BANG,     "!");  continue; }
        if (peek() == ';') { adv(); add(t,&n,T_SEMICOLON,";");  continue; }
        if (peek() == '(') { adv(); add(t,&n,T_LPAREN,   "(");  continue; }
        if (peek() == ')') { adv(); add(t,&n,T_RPAREN,   ")");  continue; }
        if (peek() == '{') { adv(); add(t,&n,T_LBRACE,   "{");  continue; }
        if (peek() == '}') { adv(); add(t,&n,T_RBRACE,   "}");  continue; }
        if (peek() == '[') { adv(); add(t,&n,T_LBRACKET, "[");  continue; }
        if (peek() == ']') { adv(); add(t,&n,T_RBRACKET, "]");  continue; }
        if (peek() == ',') { adv(); add(t,&n,T_COMMA,    ",");  continue; }

        /* string literal */
        if (peek() == '"') {
            adv(); /* skip opening quote */
            char buf[256]; int j = 0;
            while (peek() && peek() != '"') {
                if (peek() == '\\') {
                    adv();
                    char esc = adv();
                    if (j < 254) {
                        buf[j++] = '\\';
                        buf[j++] = esc;
                    }
                } else {
                    if (j < 255) buf[j++] = adv(); else adv();
                }
            }
            if (peek() == '"') adv(); /* skip closing quote */
            buf[j] = '\0';
            add(t, &n, T_STRING, buf);
            continue;
        }

        /* number: int or float */
        if (isdigit((unsigned char)peek())) {
            char buf[64]; int j = 0;
            int is_float = 0;
            while (isdigit((unsigned char)peek()) && j < 63) buf[j++] = adv();
            if (peek() == '.' && isdigit((unsigned char)peek2())) {
                is_float = 1;
                buf[j++] = adv(); /* dot */
                while (isdigit((unsigned char)peek()) && j < 63) buf[j++] = adv();
            }
            buf[j] = '\0';
            add(t, &n, is_float ? T_FLOAT : T_INT, buf);
            continue;
        }

        /* identifier or keyword */
        if (isalpha((unsigned char)peek()) || peek() == '_') {
            char buf[64]; int j = 0;
            while ((isalnum((unsigned char)peek()) || peek() == '_') && j < 63)
                buf[j++] = adv();
            buf[j] = '\0';
            add(t, &n, keyword_type(buf), buf);
            continue;
        }

        /* skip unknown chars */
        adv();
    }

    add(t, &n, T_EOF, "");
    return n;
}