#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

// Token stream
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

// Package registry
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

// Variable types
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

// Declarations forward
static void parse_block(FILE *out);
static void parse_stmt(FILE *out);
static VarType parse_expr(FILE *out);

// Package block
static int packages_done  = 0;
static int g_auto_install = 0;

static int pkg_on_disk(const char *pkg) {
    char path[512];
    snprintf(path, sizeof(path), "std/%s", pkg);
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int pkg_runtime_on_disk(const char *pkg) {
    char path[512];
    struct stat st;
    snprintf(path, sizeof(path), "std/%s/src/runtime.c", pkg);
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void emit_runtime_file(FILE *out, const char *path) {
    FILE *rf = fopen(path, "r");
    if (!rf) {
        fprintf(stderr, "error: missing package runtime '%s'\n", path);
        exit(1);
    }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), rf)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(rf);
    fprintf(out, "\n");
}

static void emit_imported_package_runtimes(FILE *out) {
    char path[512];
    for (int k = 0; k < pkg_count; k++) {
        if (!pkg_runtime_on_disk(imported[k])) {
            continue;
        }
        snprintf(path, sizeof(path), "std/%s/src/runtime.c", imported[k]);
        emit_runtime_file(out, path);
    }
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

// Expression parser

static VarType parse_unary(FILE *out);
static VarType parse_pkg_member_call(FILE *out, const char *pkg, const char *member);

static int parse_call_args(char args[][256], VarType arg_types[], int max_args) {
    int argc = 0;
    if (cur().type != T_RPAREN) {
        while (1) {
            if (argc >= max_args) {
                fprintf(stderr, "error: too many function arguments (max %d)\n", max_args);
                exit(1);
            }
            char tmp[256] = {0};
            FILE *tf = fmemopen(tmp, sizeof(tmp), "w");
            arg_types[argc] = parse_expr(tf);
            fclose(tf);
            tmp[255] = '\0';
            strncpy(args[argc], tmp, 255);
            args[argc][255] = '\0';
            argc++;

            if (cur().type == T_COMMA) {
                adv();
                continue;
            }
            break;
        }
    }
    expect(T_RPAREN, "expected ')' in function call");
    return argc;
}

static VarType parse_pkg_member_call(FILE *out, const char *pkg, const char *member) {
    char args[8][256];
    VarType arg_types[8];
    int argc = parse_call_args(args, arg_types, 8);

    if (strcmp(pkg, "math") == 0) {
        require_pkg("math", "math package call");
        if (strcmp(member, "add") == 0 && argc == 2) {
            fprintf(out, "ccpl_math_add(%s, %s)", args[0], args[1]);
            return (arg_types[0] == VT_FLOAT || arg_types[1] == VT_FLOAT) ? VT_FLOAT : VT_INT;
        }
        if (strcmp(member, "sub") == 0 && argc == 2) {
            fprintf(out, "ccpl_math_sub(%s, %s)", args[0], args[1]);
            return (arg_types[0] == VT_FLOAT || arg_types[1] == VT_FLOAT) ? VT_FLOAT : VT_INT;
        }
        if (strcmp(member, "mul") == 0 && argc == 2) {
            fprintf(out, "ccpl_math_mul(%s, %s)", args[0], args[1]);
            return (arg_types[0] == VT_FLOAT || arg_types[1] == VT_FLOAT) ? VT_FLOAT : VT_INT;
        }
        if (strcmp(member, "div") == 0 && argc == 2) {
            fprintf(out, "ccpl_math_div(%s, %s)", args[0], args[1]);
            return VT_FLOAT;
        }
        if (strcmp(member, "mod") == 0 && argc == 2) {
            fprintf(out, "ccpl_math_mod((int)(%s), (int)(%s))", args[0], args[1]);
            return VT_INT;
        }
        if (strcmp(member, "pow") == 0 && argc == 2) {
            fprintf(out, "ccpl_math_pow(%s, %s)", args[0], args[1]);
            return VT_FLOAT;
        }
        fprintf(stderr, "error: unsupported math call '%s.%s'\n", pkg, member);
        exit(1);
    }

    if (strcmp(pkg, "io") == 0) {
        require_pkg("io", "io package call");
        if (strcmp(member, "print") == 0 && argc == 1) {
            switch (arg_types[0]) {
                case VT_INT:
                case VT_ARRAY_INT:
                    fprintf(out, "(ccpl_io_print_int((int)(%s)), 0)", args[0]);
                    break;
                case VT_FLOAT:
                case VT_ARRAY_FLOAT:
                    fprintf(out, "(ccpl_io_print_float((double)(%s)), 0)", args[0]);
                    break;
                case VT_STRING:
                case VT_ARRAY_STRING:
                    fprintf(out, "(ccpl_io_print_str(%s), 0)", args[0]);
                    break;
            }
            return VT_INT;
        }
        if (strcmp(member, "len") == 0 && argc == 1) {
            fprintf(out, "ccpl_io_len(%s)", args[0]);
            return VT_INT;
        }
        fprintf(stderr, "error: unsupported io call '%s.%s'\n", pkg, member);
        exit(1);
    }

    if (strcmp(pkg, "shell") == 0) {
        require_pkg("shell", "shell package call");
        if (strcmp(member, "capture") == 0 && argc == 1) {
            fprintf(out, "ccpl_shell(%s)", args[0]);
            return VT_STRING;
        }
        if (strcmp(member, "exec") == 0 && argc == 1) {
            fprintf(out, "(ccpl_shell_exec(%s), 0)", args[0]);
            return VT_INT;
        }
        fprintf(stderr, "error: unsupported shell call '%s.%s'\n", pkg, member);
        exit(1);
    }

    if (strcmp(pkg, "str") == 0) {
        require_pkg("str", "str package call");
        char cname[192];
        snprintf(cname, sizeof(cname), "ccpl_%s_%s", pkg, member);
        fprintf(out, "%s(", cname);
        for (int i = 0; i < argc; i++) {
            if (i) fprintf(out, ", ");
            fprintf(out, "%s", args[i]);
        }
        fprintf(out, ")");

        if (strcmp(member, "contains") == 0 || strcmp(member, "starts_with") == 0 ||
            strcmp(member, "ends_with") == 0 || strcmp(member, "index_of") == 0 ||
            strcmp(member, "to_int") == 0) {
            return VT_INT;
        }
        if (strcmp(member, "to_float") == 0) {
            return VT_FLOAT;
        }
        if (strcmp(member, "trim") == 0 || strcmp(member, "lower") == 0 || strcmp(member, "upper") == 0) {
            return VT_STRING;
        }
        return VT_FLOAT;
    }

    if (strcmp(pkg, "rand") == 0) {
        require_pkg("rand", "rand package call");
        char cname[192];
        snprintf(cname, sizeof(cname), "ccpl_%s_%s", pkg, member);
        fprintf(out, "%s(", cname);
        for (int i = 0; i < argc; i++) {
            if (i) fprintf(out, ", ");
            fprintf(out, "%s", args[i]);
        }
        fprintf(out, ")");

        if (strcmp(member, "seed") == 0 || strcmp(member, "int") == 0 || strcmp(member, "choice_idx") == 0) {
            return VT_INT;
        }
        if (strcmp(member, "float") == 0) {
            return VT_FLOAT;
        }
        return VT_FLOAT;
    }

    if (strcmp(pkg, "dt") == 0) {
        require_pkg("dt", "dt package call");
        char cname[192];
        snprintf(cname, sizeof(cname), "ccpl_%s_%s", pkg, member);
        fprintf(out, "%s(", cname);
        for (int i = 0; i < argc; i++) {
            if (i) fprintf(out, ", ");
            fprintf(out, "%s", args[i]);
        }
        fprintf(out, ")");

        if (strcmp(member, "now_iso") == 0 || strcmp(member, "format_unix") == 0) {
            return VT_STRING;
        }
        return VT_FLOAT;
    }

    if (strcmp(pkg, "crypto") == 0) {
        require_pkg("crypto", "crypto package call");
        char cname[192];
        snprintf(cname, sizeof(cname), "ccpl_%s_%s", pkg, member);
        fprintf(out, "%s(", cname);
        for (int i = 0; i < argc; i++) {
            if (i) fprintf(out, ", ");
            fprintf(out, "%s", args[i]);
        }
        fprintf(out, ")");
        return VT_STRING;
    }

    if (!pkg_imported(pkg)) {
        fprintf(stderr, "error: package '%s' is not imported\n", pkg);
        exit(1);
    }

    char cname[192];
    snprintf(cname, sizeof(cname), "ccpl_%s_%s", pkg, member);
    fprintf(out, "%s(", cname);
    for (int i = 0; i < argc; i++) {
        if (i) fprintf(out, ", ");
        fprintf(out, "%s", args[i]);
    }
    fprintf(out, ")");
    return VT_FLOAT;
}

static VarType parse_primary(FILE *out) {

    // integer literal
    if (cur().type == T_INT) {
        fprintf(out, "%s", cur().value);
        adv();
        return VT_INT;
    }

    // float literal
    if (cur().type == T_FLOAT) {
        fprintf(out, "%s", cur().value);
        adv();
        return VT_FLOAT;
    }

    // boolean literals
    if (cur().type == T_TRUE)  { adv(); fprintf(out, "1"); return VT_INT; }
    if (cur().type == T_FALSE) { adv(); fprintf(out, "0"); return VT_INT; }

    // string literal
    if (cur().type == T_STRING) {
        require_pkg("io", "string literal");
        fprintf(out, "\"%s\"", cur().value);
        adv();
        return VT_STRING;
    }

    // array literal
    if (cur().type == T_LBRACKET) {
        require_pkg("math", "array literal");
        adv(); /* [ */
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
        (void)arr_vt;
        return arr_vt;
    }

    // len(expr)
    if (cur().type == T_LEN) {
        require_pkg("io", "len()");
        adv(); /* len */
        expect(T_LPAREN, "expected '(' after len");
        char tmp[256]; FILE *tf = fmemopen(tmp, sizeof(tmp), "w");
        VarType t = parse_expr(tf);
        fclose(tf); tmp[255] = '\0';
        expect(T_RPAREN, "expected ')' after len argument");
        (void)t;
        fprintf(out, "ccpl_io_len(%s)", tmp);
        return VT_INT;
    }

    // shell capture
    if (cur().type == T_DOLLAR_PAREN) {
        require_pkg("shell", "shell capture $()");
        fprintf(out, "ccpl_shell(\"%s\")", cur().value);
        adv();
        return VT_STRING;
    }

    // function calls
    if (cur().type == T_IDENT) {
        char name[64];
        strncpy(name, cur().value, 63); name[63] = '\0';
        adv();

        // package member call: pkg.member(...)
        if (cur().type == T_DOT) {
            adv();
            Token member_tok = cur();
            if (member_tok.type != T_IDENT && member_tok.type != T_PRINT && member_tok.type != T_LEN) {
                fprintf(stderr, "error: expected member name after '.' (got '%s')\n", cur().value);
                exit(1);
            }
            adv();
            expect(T_LPAREN, "expected '(' after package member name");
            return parse_pkg_member_call(out, name, member_tok.value);
        }

        // function call
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

        // array index
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

        // post inc/dec
        if (cur().type == T_PLUSPLUS)   { adv(); fprintf(out, "%s++", name); return VT_INT; }
        if (cur().type == T_MINUSMINUS) { adv(); fprintf(out, "%s--", name); return VT_INT; }

        fprintf(out, "%s", name);
        return var_get(name);
    }

    // parenthesised expression
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

// unary expressions
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

// Power expressions
static VarType parse_power(FILE *out) {
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
        fprintf(out, "ccpl_math_pow(%s, %s)", lhs, rhs);
        return VT_FLOAT;
    }

    fprintf(out, "%s", lhs);
    return lt;
}

// Mult
static VarType parse_mul(FILE *out) {
    char acc[4096] = {0};
    FILE *af = fmemopen(acc, sizeof(acc), "w");
    VarType t = parse_power(af);
    fclose(af);
    acc[sizeof(acc)-1] = '\0';

    while (cur().type == T_STAR || cur().type == T_SLASH || cur().type == T_PERCENT) {
        require_pkg("math", "arithmetic");
        TokenType op = cur().type;
        adv();

        char rhs[2048] = {0};
        FILE *rf = fmemopen(rhs, sizeof(rhs), "w");
        VarType r = parse_power(rf);
        fclose(rf);
        rhs[sizeof(rhs)-1] = '\0';

        char next[4096] = {0};
        if (op == T_STAR) {
            snprintf(next, sizeof(next), "ccpl_math_mul(%s, %s)", acc, rhs);
            if (t == VT_FLOAT || r == VT_FLOAT) t = VT_FLOAT;
        } else if (op == T_SLASH) {
            snprintf(next, sizeof(next), "ccpl_math_div(%s, %s)", acc, rhs);
            t = VT_FLOAT;
        } else {
            snprintf(next, sizeof(next), "ccpl_math_mod((int)(%s), (int)(%s))", acc, rhs);
            t = VT_INT;
        }

        strncpy(acc, next, sizeof(acc)-1);
        acc[sizeof(acc)-1] = '\0';
    }

    fprintf(out, "%s", acc);
    return t;
}

// Additive
static VarType parse_add(FILE *out) {
    char acc[4096] = {0};
    FILE *af = fmemopen(acc, sizeof(acc), "w");
    VarType t = parse_mul(af);
    fclose(af);
    acc[sizeof(acc)-1] = '\0';

    while (cur().type == T_PLUS || cur().type == T_MINUS) {
        TokenType op = cur().type;
        adv();

        char rhs[2048] = {0};
        FILE *rf = fmemopen(rhs, sizeof(rhs), "w");
        VarType r = parse_mul(rf);
        fclose(rf);
        rhs[sizeof(rhs)-1] = '\0';

        char next[4096] = {0};
        if (op == T_PLUS && (t == VT_STRING || r == VT_STRING)) {
            require_pkg("io", "string concatenation");
            snprintf(next, sizeof(next), "ccpl_strcat_fn(%s, %s)", acc, rhs);
            t = VT_STRING;
        } else {
            require_pkg("math", "arithmetic");
            if (op == T_PLUS)
                snprintf(next, sizeof(next), "ccpl_math_add(%s, %s)", acc, rhs);
            else
                snprintf(next, sizeof(next), "ccpl_math_sub(%s, %s)", acc, rhs);

            if (t == VT_FLOAT || r == VT_FLOAT) t = VT_FLOAT;
            else t = VT_INT;
        }

        strncpy(acc, next, sizeof(acc)-1);
        acc[sizeof(acc)-1] = '\0';
    }

    fprintf(out, "%s", acc);
    return t;
}

// Comparisons
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

// logical AND: &&
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

// logical OR: ||
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

// Statement parser

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
        case VT_ARRAY_INT:    fprintf(out, "ccpl_io_print_int((int)(%s));\n",    tmp); break;
        case VT_FLOAT:
        case VT_ARRAY_FLOAT:  fprintf(out, "ccpl_io_print_float((double)(%s));\n", tmp); break;
        case VT_STRING:
        case VT_ARRAY_STRING: fprintf(out, "ccpl_io_print_str(%s);\n",           tmp); break;
    }
}

static void parse_return(FILE *out) {
    fprintf(out, "return (double)(");
    parse_expr(out);
    fprintf(out, ");\n");
}

static void parse_shell_stmt(FILE *out) {
    require_pkg("shell", "shell command $()");
    fprintf(out, "ccpl_shell_exec(\"%s\");\n", cur().value);
    adv();
}

static void parse_assign_or_expr(FILE *out) {
    // array element assignment
    if (cur().type == T_IDENT && peek_ahead().type == T_LBRACKET) {
        int save = pos;
        char name[64];
        strncpy(name, cur().value, 63); name[63] = '\0';
        adv();
        adv();
        // skip index expression tokens to find ] =
        int depth = 1;
        while (cur().type != T_EOF && depth > 0) {
            if (cur().type == T_LBRACKET) depth++;
            if (cur().type == T_RBRACKET) depth--;
            if (depth > 0) adv(); else break;
        }
        adv(); // ]
        if (cur().type == T_EQ) {
            pos = save;
            adv();
            fprintf(out, "%s[", name);
            adv();
            parse_expr(out);
            expect(T_RBRACKET, "expected ']' in indexed assignment");
            fprintf(out, "] = ");
            adv();
            parse_expr(out);
            fprintf(out, ";\n");
            return;
        }
        pos = save;
    }

    // simple assignment
    if (cur().type == T_IDENT && peek_ahead().type == T_EQ) {
        char name[64];
        strncpy(name, cur().value, 63); name[63] = '\0';
        adv();
        adv();

        char tmp[1024];
        FILE *tmp_f = fmemopen(tmp, sizeof(tmp), "w");
        VarType t = parse_expr(tmp_f);
        fclose(tmp_f);
        tmp[sizeof(tmp)-1] = '\0';

        // determine C type
        const char *ctype;
        int elem_count = 0;
        // detect array literal size from tmp (count commas at depth 0 inside {})
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

            if (t == VT_ARRAY_INT || t == VT_ARRAY_FLOAT || t == VT_ARRAY_STRING) {
                // declare a fixed-size array and initialise from compound literal
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

    // Plain statement
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

// Public entry point

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
        "/* package runtimes are injected from std/<pkg>/src/runtime.c */\n\n"
    );

    // forward-declare all functions
    for (int k = 0; t[k].type != T_EOF; k++) {
        if (t[k].type == T_DEF && t[k+1].type != T_EOF && t[k+1].type == T_IDENT)
            fprintf(out, "double %s();\n", t[k+1].value);
    }
    fprintf(out, "\n");

    // pass 1: package scan
    for (int k = 0; t[k].type != T_EOF; k++) {
        if (t[k].type == T_PACKAGES) {
            pos = k + 1;
            parse_packages();
            break;
        }
    }

    emit_imported_package_runtimes(out);

    // pass 1b: emit def bodies
    pos = 0;
    while (cur().type != T_EOF) {
        if (cur().type == T_DEF) { adv(); parse_def(out); }
        else adv();
    }

    // pass 2: emit main()
    pos = 0;
    var_count = 0;
    fprintf(out, "\nint main(void) {\n");

    while (cur().type != T_EOF) {
        if (cur().type == T_DEF) {
            adv(); adv(); adv();
            while (cur().type != T_RPAREN && cur().type != T_EOF) adv();
            adv();
            skip_block();
        } else {
            parse_stmt(out);
        }
    }

    fprintf(out, "return 0;\n}\n");
    return 0;
}