#ifndef AST_H
#define AST_H

typedef enum {
    AST_LET,
    AST_PRINT,
    AST_BINOP,
    AST_NUM,
    AST_VAR
} ASTType;

typedef struct AST {
    ASTType type;

    char name[64];
    int value;
    char op;

    struct AST *left;
    struct AST *right;
} AST;

#endif