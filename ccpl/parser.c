#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

/* ─────────────────────────────────────────────
   TOKEN STREAM
───────────────────────────────────────────── */
static Token *T;
static int    pos;

static Token  cur()        { return T[pos]; }
static Token  peek_ahead() { return T[pos].type != T_EOF ? T[pos+1] : T[pos]; }
static Token  adv()        { Token t = T[pos]; if (t.type != T_EOF) pos++; return t; }

static int match(TokenType type) {
    if (cur().type == type) { adv(); return 1; }
    return 0;
}

static Token expect(TokenType type, const char *msg) {
    if (cur().type != type) {
        fprintf(stderr, "error: %s (got '%s')\n", msg, cur().value);
        exit(1);
    }
    return adv();
}

/* ─────────────────────────────────────────────
   PACKAGE REGISTRY
───────────────────────────────────────────── */
#define MAX_PKGS 64
static char imported[MAX_PKGS][64];
static int  pkg_count = 0;

static void pkg_register(const char *name) {
    if (pkg_count < MAX_PKGS) {
        strncpy(imported[pkg_count], name, 63);
        imported[pkg_count][63] = '\0';
        pkg_count++;
    }
}

static int pkg_imported(const char *name) {
    for (int k = 0; k < pkg_count; k++)
        if (strcmp(imported[k], name) == 0) return 1;
    return 0;
}

static void require_pkg(const char *pkg, const char *feature) {
    if (!pkg_imported(pkg)) {
        fprintf(stderr,
            "error: '%s' requires package '%s' "
            "(add '%s' to your packages block)\n",
            feature, pkg, pkg);
        exit(1);
    }
}

/* ─────────────────────────────────────────────
   VARIABLE TYPE TRACKING
───────────────────────────────────────────── */
typedef enum { VT_INT, VT_FLOAT, VT_STRING, VT_ARRAY_INT, VT_ARRAY_FLOAT, VT_ARRAY_STRING } VarType;

#define MAX_VARS 256
static struct { char name[64]; VarType type; } vars[MAX_VARS];
static int var_count = 0;

static void var_set(const char *name, VarType type) {
    for (int k = 0; k < var_count; k++) {
        if (strcmp(vars[k].name, name) == 0) { vars[k].type = type; return; }
    }
    if (var_count < MAX_VARS) {
        strncpy(vars[var_count].name, name, 63);
        vars[var_count].name[63] = '\0';
        vars[var_count].type = type;
        var_count++;
    }
}

static VarType var_get(const char *name) {
    for (int k = 0; k < var_count; k++)
        if (strcmp(vars[k].name, name) == 0) return vars[k].type;
    return VT_INT;
}

static int var_exists(const char *name) {
    for (int k = 0; k < var_count; k++)
        if (strcmp(vars[k].name, name) == 0) return 1;
    return 0;
}

/* ─────────────────────────────────────────────
   FORWARD DECLARATIONS
───────────────────────────────────────────── */
static void parse_block(FILE *out);
static void parse_stmt(FILE *out);
static VarType parse_expr(FILE *out);

/* ─────────────────────────────────────────────
   PACKAGE BLOCK
───────────────────────────────────────────── */
static int packages_done  = 0;
static int g_auto_install = 0;

static int pkg_on_disk(const char *pkg) {
    char path[512];
    snprintf(path, sizeof(path), "std/%s", pkg);
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void parse_packages(void) {
    match(T_EQ);
    expect(T_LBRACE, "expected '{' after packages");

    while (cur().type != T_RBRACE && cur().type != T_EOF) {
        if (cur().type == T_IDENT) {
            char pkg[64];
            strncpy(pkg, cur().value, 63); pkg[63] = '\0';
            adv();
            if (cur().type == T_EQ) { adv(); adv(); }

            if (!packages_done) {
                if (!pkg_on_disk(pkg)) {
                    if (g_auto_install) {
                        char cmd[256];
                        snprintf(cmd, sizeof(cmd), "./barite-cli install local %s", pkg);
                        int rc = system(cmd);
                        if (rc != 0 || !pkg_on_disk(pkg)) {
                            fprintf(stderr,
                                "error: package '%s' could not be installed\n"
                                "  run: ./barite-cli install local %s\n", pkg, pkg);
                            exit(1);
                        }
                    } else {
                        fprintf(stderr,
                            "error: package '%s' is not installed\n"
                            "  run: ./barite-cli install local %s\n"
                            "  or recompile with --auto / -a to install automatically\n",
                            pkg, pkg);
                        exit(1);
                    }
                }
            }

            pkg_register(pkg);
        } else {
            match(T_COMMA);
        }
    }
    expect(T_RBRACE, "expected '}' to close packages block");
    packages_done = 1;
}

/* ─────────────────────────────────────────────
   EXPRESSION PARSER
───────────────────────────────────────────── */

/* forward declare unary so power can call it */
static VarType parse_unary(FILE *out);

/* primary: literal, array literal, variable/index, len(), call, $(), ( expr ) */
static VarType parse_primary(FILE *out) {

    /* integer literal */
    if (cur().type == T_INT) {
        fprintf(out, "%s", cur().value);
        adv();
        return VT_INT;
    }

    /* float literal */
    if (cur().type == T_FLOAT) {
        fprintf(out, "%s", cur().value);
        adv();
        return VT_FLOAT;
    }

    /* boolean literals */
    if (cur().type == T_TRUE)  { adv(); fprintf(out, "1"); return VT_INT; }
    if (cur().type == T_FALSE) { adv(); fprintf(out, "0"); return VT_INT; }

    /* string literal */
    if (cur().type == T_STRING) {
        require_pkg("io", "string literal");
        fprintf(out, "\"%s\"", cur().value);
        adv();
        return VT_STRING;
    }

    /* array literal: [ expr, expr, ... ] */
    if (cur().type == T_LBRACKET) {
        require_pkg("math", "array literal");
        adv(); /* [ */
        /* We emit a compound literal — caller must handle assignment context.
           Determine element type from first element. */
        char elems[32][256];
        VarType etypes[32];
        int ecount = 0;
        while (cur().type != T_RBRACKET && cur().type != T_EOF && ecount < 32) {
            char tmp[256]; FILE *tf = fmemopen(tmp, sizeof(tmp), "w");
            etypes[ecount] = parse_expr(tf);
            fclose(tf); tmp[255] = '\0';
            strncpy(elems[ecount], tmp, 255);
            ecount++;
            if (cur().type == T_COMMA) adv();
        }
        expect(T_RBRACKET, "expected ']' to close array literal");
        /* Pick C type from element types */
        VarType arr_vt = VT_ARRAY_INT;
        const char *ctype = "int";
        for (int k = 0; k < ecount; k++) {
            if (etypes[k] == VT_FLOAT)  { arr_vt = VT_ARRAY_FLOAT;  ctype = "double"; }
            if (etypes[k] == VT_STRING) { arr_vt = VT_ARRAY_STRING; ctype = "const char*"; }
        }
        fprintf(out, "{");
        for (int k = 0; k < ecount; k++) {
            if (k) fprintf(out, ", ");
            fprintf(out, "%s", elems[k]);
        }
        fprintf(out, "}");
        (void)arr_vt; /* returned via caller's var tracking */
        return arr_vt;
    }

    /* len(expr) */
    if (cur().type == T_LEN) {
        require_pkg("io", "len()");
        adv(); /* len */
        expect(T_LPAREN, "expected '(' after len");
        char tmp[256]; FILE *tf = fmemopen(tmp, sizeof(tmp), "w");
        VarType t = parse_expr(tf);
        fclose(tf); tmp[255] = '\0';
        expect(T_RPAREN, "expected ')' after len argument");
        if (t == VT_STRING) {
            fprintf(out, "(int)strlen(%s)", tmp);
        } else {
            /* arrays: can't easily get length at runtime without extra tracking,
               so we emit a sizeof trick only for literals — warn otherwise */
            fprintf(out, "(int)strlen(%s)", tmp);
        }
        return VT_INT;
    }

    /* shell capture: $( cmd ) */
    if (cur().type == T_DOLLAR_PAREN) {
        require_pkg("shell", "shell capture $()");
        fprintf(out, "ccpl_shell(\"%s\")", cur().value);
        adv();
        return VT_STRING;
    }

    /* function call, array index, variable */
    if (cur().type == T_IDENT) {
        char name[64];
        strncpy(name, cur().value, 63); name[63] = '\0';
        adv();

        /* function call */
        if (cur().type == T_LPAREN) {
            adv();
            fprintf(out, "%s(", name);
            if (cur().type != T_RPAREN) {
                parse_expr(out);
                while (cur().type == T_COMMA) {
                    adv();
                    fprintf(out, ", ");
                    parse_expr(out);
                }
            }
            expect(T_RPAREN, "expected ')' in function call");
            fprintf(out, ")");
            return VT_INT;
        }

        /* array index: name[expr] */
        if (cur().type == T_LBRACKET) {
            adv(); /* [ */
            fprintf(out, "%s[", name);
            parse_expr(out);
            expect(T_RBRACKET, "expected ']' after array index");
            fprintf(out, "]");
            VarType vt = var_get(name);
            if (vt == VT_ARRAY_FLOAT)  return VT_FLOAT;
            if (vt == VT_ARRAY_STRING) return VT_STRING;
            return VT_INT;
        }

        /* post-increment / post-decrement */
        if (cur().type == T_PLUSPLUS)   { adv(); fprintf(out, "%s++", name); return VT_INT; }
        if (cur().type == T_MINUSMINUS) { adv(); fprintf(out, "%s--", name); return VT_INT; }

        fprintf(out, "%s", name);
        return var_get(name);
    }

    /* parenthesised expression */
    if (cur().type == T_LPAREN) {
        adv();
        fprintf(out, "(");
        VarType t = parse_expr(out);
        expect(T_RPAREN, "expected ')'");
        fprintf(out, ")");
        return t;
    }

    fprintf(stderr, "error: unexpected token '%s' in expression\n", cur().value);
    exit(1);
}

/* unary: - expr   !expr */
static VarType parse_unary(FILE *out) {
    if (cur().type == T_MINUS) {
        adv();
        fprintf(out, "-(");
        VarType t = parse_unary(out);
        fprintf(out, ")");
        return t;
    }
    if (cur().type == T_BANG) {
        adv();
        fprintf(out, "!(");
        parse_unary(out);
        fprintf(out, ")");
        return VT_INT;
    }
    return parse_primary(out);
}

/* power: expr ** expr  or  expr ^ expr  (right-assoc via pow()) */
static VarType parse_power(FILE *out) {
    /* we need to buffer lhs to wrap in pow() if ** follows */
    char lhs[512];
    FILE *lf = fmemopen(lhs, sizeof(lhs), "w");
    VarType lt = parse_unary(lf);
    fclose(lf); lhs[511] = '\0';

    if (cur().type == T_STARSTAR || cur().type == T_CARET) {
        require_pkg("math", "power operator");
        adv();
        char rhs[512];
        FILE *rf = fmemopen(rhs, sizeof(rhs), "w");
        parse_power(rf); /* right-associative */
        fclose(rf); rhs[511] = '\0';
        fprintf(out, "pow(%s, %s)", lhs, rhs);
        return VT_FLOAT;
    }

    fprintf(out, "%s", lhs);
    return lt;
}

/* multiplicative: * / % */
static VarType parse_mul(FILE *out) {
    VarType t = parse_power(out);
    while (cur().type == T_STAR || cur().type == T_SLASH || cur().type == T_PERCENT) {
        require_pkg("math", "arithmetic");
        TokenType op = cur().type;
        adv();
        if (op == T_STAR)    fprintf(out, " * ");
        else if (op == T_SLASH)   fprintf(out, " / ");
        else                      fprintf(out, " %% ");
        VarType r = parse_power(out);
        if (t == VT_FLOAT || r == VT_FLOAT) t = VT_FLOAT;
        if (op == T_PERCENT) t = VT_INT; /* modulo always int */
    }
    return t;
}

/* additive: + -  (also string concat with +) */
static VarType parse_add(FILE *out) {
    VarType t = parse_mul(out);
    while (cur().type == T_PLUS || cur().type == T_MINUS) {
        TokenType op = cur().type;

        /* string concat: use a helper emitted in preamble */
        if (op == T_PLUS && t == VT_STRING) {
            require_pkg("io", "string concatenation");
            adv();
            char rhs[512]; FILE *rf = fmemopen(rhs, sizeof(rhs), "w");
            parse_mul(rf);
            fclose(rf); rhs[511] = '\0';
            /* wrap lhs in a concat call — rewrite what we already emitted.
               We can't easily undo fprintf, so we buffer the whole add chain. */
            fprintf(out, " /* str+: use strcat */ ");
            /* Emit as runtime strcat into a static buffer */
            fprintf(out, "ccpl_strcat(%s)", rhs);
            return VT_STRING;
        }

        if (op == T_PLUS) require_pkg("math", "arithmetic");
        adv();
        fprintf(out, op == T_PLUS ? " + " : " - ");
        VarType r = parse_mul(out);
        if (t == VT_FLOAT || r == VT_FLOAT) t = VT_FLOAT;
    }
    return t;
}

/* comparison: == != < > <= >= */
static VarType parse_cmp(FILE *out) {
    VarType t = parse_add(out);
    while (cur().type == T_EQEQ || cur().type == T_NEQ ||
           cur().type == T_LT   || cur().type == T_GT  ||
           cur().type == T_LTE  || cur().type == T_GTE) {
        const char *op;
        switch (cur().type) {
            case T_EQEQ: op = "=="; break;
            case T_NEQ:  op = "!="; break;
            case T_LT:   op = "<";  break;
            case T_GT:   op = ">";  break;
            case T_LTE:  op = "<="; break;
            case T_GTE:  op = ">="; break;
            default:     op = "?";
        }
        adv();
        fprintf(out, " %s ", op);
        parse_add(out);
        t = VT_INT;
    }
    return t;
}

/* logical AND: && */
static VarType parse_and(FILE *out) {
    VarType t = parse_cmp(out);
    while (cur().type == T_AND) {
        adv();
        fprintf(out, " && ");
        parse_cmp(out);
        t = VT_INT;
    }
    return t;
}

/* logical OR: || */
static VarType parse_or(FILE *out) {
    VarType t = parse_and(out);
    while (cur().type == T_OR) {
        adv();
        fprintf(out, " || ");
        parse_and(out);
        t = VT_INT;
    }
    return t;
}

static VarType parse_expr(FILE *out) {
    return parse_or(out);
}

/* ─────────────────────────────────────────────
   STATEMENT PARSER
───────────────────────────────────────────── */

static void parse_if(FILE *out) {
    fprintf(out, "if (");
    parse_expr(out);
    fprintf(out, ") {\n");
    expect(T_LBRACE, "expected '{' after if condition");
    parse_block(out);
    expect(T_RBRACE, "expected '}' to close if block");
    fprintf(out, "}");
    if (cur().type == T_ELSE) {
        adv();
        if (cur().type == T_IF) {
            adv();
            fprintf(out, " else ");
            parse_if(out);
            return;
        }
        fprintf(out, " else {\n");
        expect(T_LBRACE, "expected '{' after else");
        parse_block(out);
        expect(T_RBRACE, "expected '}' to close else block");
        fprintf(out, "}");
    }
    fprintf(out, "\n");
}

static void parse_while(FILE *out) {
    fprintf(out, "while (");
    parse_expr(out);
    fprintf(out, ") {\n");
    expect(T_LBRACE, "expected '{' after while condition");
    parse_block(out);
    expect(T_RBRACE, "expected '}' to close while block");
    fprintf(out, "}\n");
}

static void parse_for(FILE *out) {
    char var[64];
    strncpy(var, cur().value, 63); var[63] = '\0';
    expect(T_IDENT, "expected variable in for");
    expect(T_EQ,    "expected '=' in for init");

    VarType init_type = (cur().type == T_FLOAT) ? VT_FLOAT : VT_INT;
    const char *ctype = (init_type == VT_FLOAT) ? "double" : "int";

    fprintf(out, "for (%s %s = ", ctype, var);
    parse_expr(out);
    var_set(var, init_type);

    expect(T_SEMICOLON, "expected ';' after for init");
    fprintf(out, "; ");
    parse_expr(out);
    expect(T_SEMICOLON, "expected ';' after for condition");
    fprintf(out, "; ");
    parse_expr(out);
    fprintf(out, ") {\n");
    expect(T_LBRACE, "expected '{' to open for body");
    parse_block(out);
    expect(T_RBRACE, "expected '}' to close for block");
    fprintf(out, "}\n");
}

static void parse_def(FILE *out) {
    char fname[64];
    strncpy(fname, cur().value, 63); fname[63] = '\0';
    expect(T_IDENT,  "expected function name after 'def'");
    expect(T_LPAREN, "expected '(' after function name");

    fprintf(out, "double %s(", fname);

    char params[16][64]; int param_count = 0;
    if (cur().type != T_RPAREN) {
        strncpy(params[param_count], cur().value, 63);
        params[param_count][63] = '\0';
        var_set(params[param_count++], VT_FLOAT);
        expect(T_IDENT, "expected parameter name");
        while (cur().type == T_COMMA) {
            adv();
            strncpy(params[param_count], cur().value, 63);
            params[param_count][63] = '\0';
            var_set(params[param_count++], VT_FLOAT);
            expect(T_IDENT, "expected parameter name");
        }
    }
    expect(T_RPAREN, "expected ')' after parameters");

    for (int k = 0; k < param_count; k++) {
        if (k) fprintf(out, ", ");
        fprintf(out, "double %s", params[k]);
    }
    fprintf(out, ") {\n");
    expect(T_LBRACE, "expected '{' to open function body");
    parse_block(out);
    expect(T_RBRACE, "expected '}' to close function body");
    fprintf(out, "}\n");
}

static void parse_print(FILE *out) {
    require_pkg("io", "print");
    char tmp[1024];
    FILE *tmp_f = fmemopen(tmp, sizeof(tmp), "w");
    VarType t = parse_expr(tmp_f);
    fclose(tmp_f);
    tmp[sizeof(tmp)-1] = '\0';

    switch (t) {
        case VT_INT:
        case VT_ARRAY_INT:    fprintf(out, "printf(\"%%d\\n\", (int)(%s));\n",    tmp); break;
        case VT_FLOAT:
        case VT_ARRAY_FLOAT:  fprintf(out, "printf(\"%%.6g\\n\", (double)(%s));\n", tmp); break;
        case VT_STRING:
        case VT_ARRAY_STRING: fprintf(out, "printf(\"%%s\\n\", %s);\n",           tmp); break;
    }
}

static void parse_return(FILE *out) {
    fprintf(out, "return (double)(");
    parse_expr(out);
    fprintf(out, ");\n");
}

static void parse_shell_stmt(FILE *out) {
    require_pkg("shell", "shell command $()");
    fprintf(out, "system(\"%s\");\n", cur().value);
    adv();
}

static void parse_assign_or_expr(FILE *out) {
    /* array element assignment: name[idx] = expr */
    if (cur().type == T_IDENT && peek_ahead().type == T_LBRACKET) {
        /* peek further to detect assignment */
        int save = pos;
        char name[64];
        strncpy(name, cur().value, 63); name[63] = '\0';
        adv(); /* name */
        adv(); /* [ */
        /* skip index expression tokens to find ] = */
        int depth = 1;
        while (cur().type != T_EOF && depth > 0) {
            if (cur().type == T_LBRACKET) depth++;
            if (cur().type == T_RBRACKET) depth--;
            if (depth > 0) adv(); else break;
        }
        adv(); /* ] */
        if (cur().type == T_EQ) {
            /* it is an indexed assignment */
            pos = save;
            adv(); /* name */
            fprintf(out, "%s[", name);
            adv(); /* [ */
            parse_expr(out);
            expect(T_RBRACKET, "expected ']' in indexed assignment");
            fprintf(out, "] = ");
            adv(); /* = */
            parse_expr(out);
            fprintf(out, ";\n");
            return;
        }
        /* not an assignment — rewind and fall through to expr */
        pos = save;
    }

    /* simple assignment: name = expr */
    if (cur().type == T_IDENT && peek_ahead().type == T_EQ) {
        char name[64];
        strncpy(name, cur().value, 63); name[63] = '\0';
        adv(); /* name */
        adv(); /* = */

        char tmp[1024];
        FILE *tmp_f = fmemopen(tmp, sizeof(tmp), "w");
        VarType t = parse_expr(tmp_f);
        fclose(tmp_f);
        tmp[sizeof(tmp)-1] = '\0';

        /* determine C type */
        const char *ctype;
        int elem_count = 0;
        /* detect array literal size from tmp (count commas at depth 0 inside {}) */
        if (t == VT_ARRAY_INT || t == VT_ARRAY_FLOAT || t == VT_ARRAY_STRING) {
            int d = 0;
            elem_count = 1;
            for (int k = 0; tmp[k]; k++) {
                if (tmp[k] == '{' || tmp[k] == '(') d++;
                if (tmp[k] == '}' || tmp[k] == ')') d--;
                if (tmp[k] == ',' && d == 1) elem_count++;
            }
        }

        switch (t) {
            case VT_INT:          ctype = "int";          break;
            case VT_FLOAT:        ctype = "double";        break;
            case VT_STRING:       ctype = "const char*";   break;
            case VT_ARRAY_INT:    ctype = "int";           break;
            case VT_ARRAY_FLOAT:  ctype = "double";        break;
            case VT_ARRAY_STRING: ctype = "const char*";   break;
            default:              ctype = "int";
        }

        if (!var_exists(name)) {
            if (t == VT_STRING) require_pkg("io", "string variable");
            else                require_pkg("math", "variable assignment");

            if (t == VT_ARRAY_INT || t == VT_ARRAY_FLOAT || t == VT_ARRAY_STRING) {
                /* declare a fixed-size array and initialise from compound literal */
                fprintf(out, "%s %s[] = %s;\n", ctype, name, tmp);
            } else {
                fprintf(out, "%s %s = %s;\n", ctype, name, tmp);
            }
        } else {
            fprintf(out, "%s = %s;\n", name, tmp);
        }
        var_set(name, t);
        return;
    }

    /* plain expression statement */
    parse_expr(out);
    fprintf(out, ";\n");
}

static void parse_stmt(FILE *out) {
    switch (cur().type) {
        case T_IF:       adv(); parse_if(out);          break;
        case T_WHILE:    adv(); parse_while(out);       break;
        case T_FOR:      adv(); parse_for(out);         break;
        case T_DEF:      adv(); parse_def(out);         break;
        case T_PRINT:    adv(); parse_print(out);       break;
        case T_RETURN:   adv(); parse_return(out);      break;
        case T_DOLLAR_PAREN:    parse_shell_stmt(out);  break;
        case T_PACKAGES: adv(); parse_packages();       break;
        case T_RBRACE:                                  break;
        case T_EOF:                                     break;
        default:         parse_assign_or_expr(out);    break;
    }
}

static void parse_block(FILE *out) {
    while (cur().type != T_RBRACE && cur().type != T_EOF)
        parse_stmt(out);
}

/* ─────────────────────────────────────────────
   PUBLIC ENTRY POINT
───────────────────────────────────────────── */

static void skip_block(void) {
    expect(T_LBRACE, "expected '{' in def body");
    int depth = 1;
    while (cur().type != T_EOF && depth > 0) {
        if (cur().type == T_LBRACE) depth++;
        if (cur().type == T_RBRACE) { depth--; if (depth == 0) { adv(); break; } }
        adv();
    }
}

int parse(Token *t, FILE *out, int auto_install) {
    T   = t;
    pos = 0;
    pkg_count = 0;
    var_count = 0;
    packages_done  = 0;
    g_auto_install = auto_install;

    fprintf(out,
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n"
        "#include <math.h>\n"
        "\n"
        "/* ccpl runtime */\n"
        "static char _ccpl_shell_buf[4096];\n"
        "static const char* ccpl_shell(const char *cmd) {\n"
        "    FILE *p = popen(cmd, \"r\");\n"
        "    if (!p) return \"\";\n"
        "    int n = (int)fread(_ccpl_shell_buf, 1, sizeof(_ccpl_shell_buf)-1, p);\n"
        "    pclose(p);\n"
        "    _ccpl_shell_buf[n] = '\\0';\n"
        "    if (n > 0 && _ccpl_shell_buf[n-1] == '\\n') _ccpl_shell_buf[n-1] = '\\0';\n"
        "    return _ccpl_shell_buf;\n"
        "}\n"
        "static char _ccpl_concat_buf[4096];\n"
        "static const char* ccpl_strcat_fn(const char *a, const char *b) {\n"
        "    int _la=(int)strlen(a),_lb=(int)strlen(b);\n"
        "    if(_la+_lb<4094){memcpy(_ccpl_concat_buf,a,_la);"
        "    memcpy(_ccpl_concat_buf+_la,b,_lb);"
        "    _ccpl_concat_buf[_la+_lb]=0;}\n"
        "    return _ccpl_concat_buf;\n"
        "}\n\n"
    );

    /* forward-declare all functions */
    for (int k = 0; t[k].type != T_EOF; k++) {
        if (t[k].type == T_DEF && t[k+1].type != T_EOF && t[k+1].type == T_IDENT)
            fprintf(out, "double %s();\n", t[k+1].value);
    }
    fprintf(out, "\n");

    /* pass 1: package scan */
    for (int k = 0; t[k].type != T_EOF; k++) {
        if (t[k].type == T_PACKAGES) {
            pos = k + 1;
            parse_packages();
            break;
        }
    }

    /* pass 1b: emit def bodies */
    pos = 0;
    while (cur().type != T_EOF) {
        if (cur().type == T_DEF) { adv(); parse_def(out); }
        else adv();
    }

    /* pass 2: emit main() */
    pos = 0;
    var_count = 0;
    fprintf(out, "\nint main(void) {\n");

    while (cur().type != T_EOF) {
        if (cur().type == T_DEF) {
            adv(); adv(); adv(); /* def name ( */
            while (cur().type != T_RPAREN && cur().type != T_EOF) adv();
            adv(); /* ) */
            skip_block();
        } else {
            parse_stmt(out);
        }
    }

    fprintf(out, "return 0;\n}\n");
    return 0;
}