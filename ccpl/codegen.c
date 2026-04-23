#include "codegen.h"
#include <stdio.h>

void gen(AST *n, FILE *out) {

    if (!n) return;

    if (n->type == AST_LET) {
        fprintf(out, "int %s = ", n->name);
        gen(n->left, out);
        fprintf(out, ";\n");
    }

    else if (n->type == AST_PRINT) {
        fprintf(out, "printf(\"%%d\\n\", ");
        gen(n->left, out);
        fprintf(out, ");\n");
    }

    else if (n->type == AST_NUM) {
        fprintf(out, "%d", n->value);
    }

    else if (n->type == AST_VAR) {
        fprintf(out, "%s", n->name);
    }

    else if (n->type == AST_BINOP) {
        fprintf(out, "(");
        gen(n->left, out);
        fprintf(out, " %c ", n->op);
        gen(n->right, out);
        fprintf(out, ")");
    }
}