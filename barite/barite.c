#include "barite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define CLOUD_REPO_URL   "https://github.com/QKing-Official/BariteStd.git"
#define CLOUD_CACHE_DIR  "/tmp/barite-std-cache"
#define GLOBAL_INSTALL_DIR "/opt/ccpl"

// Helpers

/* strip optional =version suffix from package name */
static void strip_version(char *pkg) {
    char *eq = strchr(pkg, '=');
    if (eq) *eq = '\0';
}

/* check if a directory exists */
static int dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* read package.barite field value, e.g. "name", "version", "description" */
static int read_field(const char *path, const char *field, char *out, int outsz) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[256];
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "%s:", field);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            char *val = line + strlen(prefix);
            while (*val == ' ' || *val == '\t') val++;
            /* strip trailing newline */
            int len = strlen(val);
            while (len > 0 && (val[len-1] == '\n' || val[len-1] == '\r')) val[--len] = '\0';
            strncpy(out, val, outsz - 1);
            out[outsz - 1] = '\0';
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

/* resolve the std/ output dir: use BARITE_STD_DIR env var if set,
   else fall back to global install dir if it exists, else use local ./std */
static void resolve_std_dir(char *out, int outsz) {
    const char *env = getenv("BARITE_STD_DIR");
    if (env && env[0]) {
        strncpy(out, env, outsz - 1);
        out[outsz - 1] = '\0';
        return;
    }
    char global_std[512];
    snprintf(global_std, sizeof(global_std), "%s/std", GLOBAL_INSTALL_DIR);
    if (dir_exists(GLOBAL_INSTALL_DIR)) {
        strncpy(out, global_std, outsz - 1);
        out[outsz - 1] = '\0';
        return;
    }
    strncpy(out, "std", outsz - 1);
    out[outsz - 1] = '\0';
}

/* resolve local-packages dir: check ./local-packages first, then global */
static void resolve_local_pkg_dir(const char *pkg, char *out, int outsz) {
    snprintf(out, outsz, "local-packages/%s", pkg);
    if (dir_exists(out)) return;
    snprintf(out, outsz, "%s/local-packages/%s", GLOBAL_INSTALL_DIR, pkg);
}

// install

int barite_install(const char *source, const char *pkg_raw) {

    char pkg[128];
    strncpy(pkg, pkg_raw, sizeof(pkg) - 1);
    pkg[sizeof(pkg) - 1] = '\0';
    strip_version(pkg);

    char src_path[512];
    char dst_path[512];
    char std_dir[512];
    char cmd[2048];

    resolve_std_dir(std_dir, sizeof(std_dir));

    if (strcmp(source, "local") == 0) {
        resolve_local_pkg_dir(pkg, src_path, sizeof(src_path));
        snprintf(dst_path, sizeof(dst_path), "%s/%s", std_dir, pkg);

    } else if (strcmp(source, "cloud") == 0) {
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", CLOUD_CACHE_DIR);
        if (system(cmd) != 0) {
            fprintf(stderr, "barite: failed to clear cloud cache directory\n");
            return 1;
        }

        snprintf(
            cmd,
            sizeof(cmd),
            "git clone --depth 1 \"%s\" \"%s\" >/dev/null 2>&1",
            CLOUD_REPO_URL,
            CLOUD_CACHE_DIR
        );
        if (system(cmd) != 0) {
            fprintf(stderr,
                "barite: failed to fetch cloud packages from %s\n"
                "  check internet connection and git availability\n",
                CLOUD_REPO_URL);
            return 1;
        }

        snprintf(src_path, sizeof(src_path), "%s/%s", CLOUD_CACHE_DIR, pkg);
        if (!dir_exists(src_path)) {
            snprintf(src_path, sizeof(src_path), "%s/std/%s", CLOUD_CACHE_DIR, pkg);
        }

        snprintf(dst_path, sizeof(dst_path), "%s/%s", std_dir, pkg);

    } else {
        fprintf(stderr, "barite: unknown source '%s' (available: local, cloud)\n", source);
        return 1;
    }

    if (!dir_exists(src_path)) {
        fprintf(stderr, "barite: package '%s' not found in %s\n", pkg, src_path);
        return 1;
    }

    /* create destination and copy */
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", dst_path);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "cp -r \"%s/\"* \"%s/\" 2>/dev/null", src_path, dst_path);
    system(cmd);

    /* read and print package info */
    char meta[512];
    char version[64] = "?";
    char description[256] = "";
    snprintf(meta, sizeof(meta), "%s/package.barite", src_path);
    read_field(meta, "version",     version,     sizeof(version));
    read_field(meta, "description", description, sizeof(description));

    printf("Installing %s package: %s", source, pkg);
    if (strcmp(version, "?") != 0) printf(" (v%s)", version);
    if (description[0])            printf(" — %s", description);
    printf("\n");

    if (strcmp(source, "cloud") == 0) {
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", CLOUD_CACHE_DIR);
        (void)system(cmd);
    }

    return 0;
}

int barite_remove(const char *pkg_raw) {

    char pkg[128];
    strncpy(pkg, pkg_raw, sizeof(pkg) - 1);
    pkg[sizeof(pkg) - 1] = '\0';
    strip_version(pkg);

    char std_dir[512];
    char dst_path[512];
    resolve_std_dir(std_dir, sizeof(std_dir));
    snprintf(dst_path, sizeof(dst_path), "%s/%s", std_dir, pkg);

    if (!dir_exists(dst_path)) {
        fprintf(stderr, "barite: package '%s' is not installed\n", pkg);
        return 1;
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", dst_path);
    system(cmd);
    printf("Removed package: %s\n", pkg);
    return 0;
}

void barite_list(void) {
    char std_dir[512];
    resolve_std_dir(std_dir, sizeof(std_dir));

    printf("Installed packages:\n");

    char find_cmd[600];
    snprintf(find_cmd, sizeof(find_cmd),
        "find \"%s\" -maxdepth 1 -mindepth 1 -type d 2>/dev/null", std_dir);

    FILE *pipe = popen(find_cmd, "r");
    if (!pipe) {
        printf("  (none)\n");
        return;
    }

    char line[512];
    int found = 0;
    while (fgets(line, sizeof(line), pipe)) {
        /* strip newline */
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';

        char meta[600];
        snprintf(meta, sizeof(meta), "%s/package.barite", line);

        char name[64]    = "";
        char version[64] = "?";
        char desc[256]   = "";
        read_field(meta, "name",        name,    sizeof(name));
        read_field(meta, "version",     version, sizeof(version));
        read_field(meta, "description", desc,    sizeof(desc));

        if (name[0] == '\0') {
            /* fallback: use dir name */
            char *slash = strrchr(line, '/');
            strncpy(name, slash ? slash + 1 : line, sizeof(name) - 1);
        }

        printf("  %-12s v%-8s %s\n", name, version, desc);
        found = 1;
    }
    pclose(pipe);

    if (!found) printf("  (none)\n");
}

void barite_info(const char *source, const char *pkg_raw) {

    char pkg[128];
    strncpy(pkg, pkg_raw, sizeof(pkg) - 1);
    pkg[sizeof(pkg) - 1] = '\0';
    strip_version(pkg);

    char src_path[512];
    char std_dir[512];
    resolve_std_dir(std_dir, sizeof(std_dir));

    if (strcmp(source, "local") == 0) {
        resolve_local_pkg_dir(pkg, src_path, sizeof(src_path));
    } else {
        /* "installed" or anything else: look in std dir */
        snprintf(src_path, sizeof(src_path), "%s/%s", std_dir, pkg);
    }

    if (!dir_exists(src_path)) {
        fprintf(stderr, "barite: package '%s' not found\n", pkg);
        return;
    }

    char meta[600];
    snprintf(meta, sizeof(meta), "%s/package.barite", src_path);

    char fields[][32] = { "name", "version", "description", "provides", "requires" };
    printf("Package info: %s\n", pkg);
    for (int i = 0; i < 5; i++) {
        char val[256] = "";
        if (read_field(meta, fields[i], val, sizeof(val)))
            printf("  %-12s %s\n", fields[i], val);
    }
}

// Usage

static void usage(void) {
    printf(
        "barite — CCPL package manager\n"
        "\n"
        "Usage:\n"
        "  barite install <pkg>         install a cloud package (default source)\n"
        "  barite install cloud <pkg>   install a cloud package\n"
        "  barite install local <pkg>   install a local package\n"
        "  barite remove <pkg>          remove an installed package\n"
        "  barite list                  list installed packages\n"
        "  barite info local <pkg>      show local package info\n"
        "  barite info installed <pkg>  show info for installed package\n"
        "\n"
        "Examples:\n"
        "  barite install math\n"
        "  barite install cloud io\n"
        "  barite install local math\n"
        "  barite install local io\n"
        "  barite install local shell\n"
        "  barite list\n"
        "  barite remove math\n"
        "\n"
        "Environment:\n"
        "  BARITE_STD_DIR   override the directory where packages are installed\n"
    );
}

// Main

int main(int argc, char **argv) {

    if (argc < 2) {
        usage();
        return 0;
    }

    const char *cmd = argv[1];

    /* install [cloud|local] <pkg> [pkg2 ...] */
    if (strcmp(cmd, "install") == 0) {
        if (argc < 3) {
            fprintf(stderr, "usage: barite install [cloud|local] <package>\n");
            return 1;
        }
        const char *source = "cloud";
        int first_pkg = 2;

        if (strcmp(argv[2], "local") == 0 || strcmp(argv[2], "cloud") == 0) {
            source = argv[2];
            first_pkg = 3;
        }

        if (first_pkg >= argc) {
            fprintf(stderr, "usage: barite install [cloud|local] <package>\n");
            return 1;
        }

        int rc = 0;
        for (int i = first_pkg; i < argc; i++)
            rc |= barite_install(source, argv[i]);
        return rc;
    }

    /* remove <pkg> [pkg2 ...] */
    if (strcmp(cmd, "remove") == 0) {
        if (argc < 3) {
            fprintf(stderr, "usage: barite remove <package>\n");
            return 1;
        }
        int rc = 0;
        for (int i = 2; i < argc; i++)
            rc |= barite_remove(argv[i]);
        return rc;
    }

    /* list */
    if (strcmp(cmd, "list") == 0) {
        barite_list();
        return 0;
    }

    /* info <source> <pkg> */
    if (strcmp(cmd, "info") == 0) {
        if (argc < 4) {
            fprintf(stderr, "usage: barite info <local|installed> <package>\n");
            return 1;
        }
        barite_info(argv[2], argv[3]);
        return 0;
    }

    fprintf(stderr, "barite: unknown command '%s'\n", cmd);
    usage();
    return 1;
}