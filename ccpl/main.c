#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include "lexer.h"
#include "parser.h"

#define MAX_SRC    16384
#define MAX_TOKENS 8192
#define REPL_SRC_MAX 262144

static volatile sig_atomic_t g_repl_interrupted = 0;
static const char *g_repl_session_file = NULL;
static const char *g_repl_out_name = NULL;

static void cleanup_repl_artifacts(void) {
    if (g_repl_session_file) {
        remove(g_repl_session_file);
    }
    if (g_repl_out_name) {
        char c_file[512];
        remove(g_repl_out_name);
        snprintf(c_file, sizeof(c_file), "%s.c", g_repl_out_name);
        remove(c_file);
    }
}

static void repl_sigint_handler(int sig) {
    (void)sig;
    g_repl_interrupted = 1;
}

static void usage(void) {
    fprintf(stderr,
        "usage: ccpl [options] <file.ccpl>\n"
        "   or: ccpl --repl\n"
        "\n"
        "options:\n"
        "  -o <name>       output binary name (default: out)\n"
        "  --auto, -a      automatically install missing packages via barite\n"
        "  --repl, -r      start interactive REPL mode\n"
        "  --quiet         suppress success output\n"
    );
}

static int compile_file_to_binary(const char *input_file, const char *output_name, int auto_install, int quiet) {
    FILE *f = fopen(input_file, "r");
    if (!f) {
        fprintf(stderr, "error: cannot open '%s'\n", input_file);
        return 1;
    }

    char *src = calloc(MAX_SRC, 1);
    if (!src) {
        fclose(f);
        return 1;
    }
    fread(src, 1, MAX_SRC - 1, f);
    fclose(f);

    Token *tokens = calloc(MAX_TOKENS, sizeof(Token));
    if (!tokens) {
        free(src);
        return 1;
    }

    lex(src, tokens);
    free(src);

    char c_file[512];
    snprintf(c_file, sizeof(c_file), "%s.c", output_name);

    FILE *out = fopen(c_file, "w");
    if (!out) {
        fprintf(stderr, "error: cannot write %s\n", c_file);
        free(tokens);
        return 1;
    }

    int result = parse(tokens, out, auto_install);
    fclose(out);
    free(tokens);

    if (result != 0) return result;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "gcc %s -o %s -lm", c_file, output_name);
    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "error: C compilation failed\n");
        return 1;
    }

    if (!quiet) {
        printf("compiled OK -> %s\n", output_name);
    }
    return 0;
}

static void trim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || isspace((unsigned char)s[n - 1]))) {
        s[--n] = '\0';
    }
}

static int is_transient_line(const char *line) {
    while (*line && isspace((unsigned char)*line)) line++;
    return strncmp(line, "print", 5) == 0 || strncmp(line, "$(", 2) == 0;
}

// The REPL, this was a pain
static int run_repl(const char *self_path, int auto_install) {
    char persistent[REPL_SRC_MAX] = {0};
    char line[2048];

    const char *session_file = ".ccpl_repl_session.ccpl";
    const char *out_name = ".ccpl_repl_bin";

    struct sigaction sa;
    struct sigaction old_sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = repl_sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    g_repl_interrupted = 0;
    g_repl_session_file = session_file;
    g_repl_out_name = out_name;
    atexit(cleanup_repl_artifacts);
    sigaction(SIGINT, &sa, &old_sa);

    printf("CCPL REPL\n");
    printf("Type :help for commands. Type :quit to exit.\n");

    for (;;) {
        if (g_repl_interrupted) {
            printf("\n");
            break;
        }

        printf("ccpl> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            if (g_repl_interrupted || errno == EINTR) {
                clearerr(stdin);
                printf("\n");
                break;
            }
            printf("\n");
            break;
        }

        if (g_repl_interrupted) {
            printf("\n");
            break;
        }

        trim(line);
        if (line[0] == '\0') continue;

        if (strcmp(line, ":quit") == 0 || strcmp(line, ":exit") == 0 ||
            strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            break;
        }
        if (strcmp(line, ":help") == 0) {
            printf(":help  show this message\n");
            printf(":clear clear REPL state\n");
            printf(":quit  exit REPL\n");
            printf("exit   exit REPL\n");
            continue;
        }
        if (strcmp(line, ":clear") == 0) {
            persistent[0] = '\0';
            printf("state cleared\n");
            continue;
        }

        int transient = is_transient_line(line);
        char source[REPL_SRC_MAX];
        if (transient) {
            snprintf(source, sizeof(source), "%s\n%s\n", persistent, line);
        } else {
            snprintf(source, sizeof(source), "%s\n", persistent);
            size_t cur_len = strlen(source);
            size_t line_len = strlen(line);
            if (cur_len + line_len + 2 >= sizeof(source)) {
                fprintf(stderr, "error: REPL buffer full, use :clear\n");
                continue;
            }
            memcpy(source + cur_len, line, line_len);
            source[cur_len + line_len] = '\n';
            source[cur_len + line_len + 1] = '\0';
        }

        FILE *sf = fopen(session_file, "w");
        if (!sf) {
            fprintf(stderr, "error: cannot write REPL session file\n");
            continue;
        }
        fputs(source, sf);
        fclose(sf);

        char compile_cmd[2048];
        snprintf(
            compile_cmd,
            sizeof(compile_cmd),
            "%s %s --quiet -o %s %s",
            self_path,
            auto_install ? "--auto" : "",
            out_name,
            session_file
        );

        int rc = system(compile_cmd);
        if (rc != 0) {
            continue;
        }

        char run_cmd[512];
        snprintf(run_cmd, sizeof(run_cmd), "./%s", out_name);
        (void)system(run_cmd);

        if (!transient) {
            size_t p_len = strlen(persistent);
            size_t l_len = strlen(line);
            if (p_len + l_len + 2 < sizeof(persistent)) {
                memcpy(persistent + p_len, line, l_len);
                persistent[p_len + l_len] = '\n';
                persistent[p_len + l_len + 1] = '\0';
            }
        }
    }

    cleanup_repl_artifacts();
    g_repl_session_file = NULL;
    g_repl_out_name = NULL;
    sigaction(SIGINT, &old_sa, NULL);
    return 0;
}

int main(int argc, char **argv) {

    const char *input_file  = NULL;
    const char *output_name = "out";
    int         auto_install = 0;
    int         repl_mode = 0;
    int         quiet = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--auto") == 0 || strcmp(argv[i], "-a") == 0) {
            auto_install = 1;
        } else if (strcmp(argv[i], "--repl") == 0 || strcmp(argv[i], "-r") == 0) {
            repl_mode = 1;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            quiet = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: -o requires an argument\n");
                usage();
                return 1;
            }
            output_name = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            usage();
            return 1;
        } else {
            if (input_file) {
                fprintf(stderr, "error: multiple input files specified\n");
                usage();
                return 1;
            }
            input_file = argv[i];
        }
    }

    if (repl_mode && input_file) {
        fprintf(stderr, "error: cannot use --repl with an input file\n");
        usage();
        return 1;
    }

    if (repl_mode) {
        return run_repl(argv[0], auto_install);
    }

    if (!input_file) {
        usage();
        return 1;
    }

    return compile_file_to_binary(input_file, output_name, auto_install, quiet);
}