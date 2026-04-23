#include "barite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#define CLOUD_REPO_URL     "https://github.com/QKing-Official/BariteStd.git"
#define CLOUD_CACHE_DIR    "/tmp/barite-std-cache"
#define GLOBAL_INSTALL_DIR "/opt/ccpl"
#define BARITE_VERSION "0.2"

// Helpers

// strip optional =version suffix from package name
static void strip_version(char *pkg) {
    char *eq = strchr(pkg, '=');
    if (eq) *eq = '\0';
}

// check if a directory exists
static int dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// read package.barite field value, e.g. "name", "version", "description"
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

// try "description" first, fall back to "desc"
static void read_description(const char *meta, char *out, int outsz) {
    out[0] = '\0';
    if (read_field(meta, "description", out, outsz) && out[0]) return;
    read_field(meta, "desc", out, outsz);
}

// resolve the std/ output dir: use BARITE_STD_DIR env var if set,
// else fall back to global install dir if it exists, else use local ./std
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

// resolve local-packages dir: check ./local-packages first, then global
static void resolve_local_pkg_dir(const char *pkg, char *out, int outsz) {
    snprintf(out, outsz, "local-packages/%s", pkg);
    if (dir_exists(out)) return;
    snprintf(out, outsz, "%s/local-packages/%s", GLOBAL_INSTALL_DIR, pkg);
}

// lowercase a string in-place
static void str_tolower(char *s) {
    for (; *s; s++) *s = tolower((unsigned char)*s);
}

// return 1 if needle (already lowercased) appears in haystack (case-insensitive)
static int icontains(const char *haystack, const char *needle_lower) {
    if (!haystack[0] || !needle_lower[0]) return 0;
    char buf[512];
    strncpy(buf, haystack, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    str_tolower(buf);
    return strstr(buf, needle_lower) != NULL;
}

// installation process

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
            cmd, sizeof(cmd),
            "git clone --depth 1 \"%s\" \"%s\" >/dev/null 2>&1",
            CLOUD_REPO_URL, CLOUD_CACHE_DIR
        );
        if (system(cmd) != 0) {
            fprintf(stderr,
                "barite: failed to fetch cloud packages from %s\n"
                "  check internet connection and git availability\n",
                CLOUD_REPO_URL);
            return 1;
        }

        snprintf(src_path, sizeof(src_path), "%s/%s", CLOUD_CACHE_DIR, pkg);
        if (!dir_exists(src_path))
            snprintf(src_path, sizeof(src_path), "%s/std/%s", CLOUD_CACHE_DIR, pkg);

        snprintf(dst_path, sizeof(dst_path), "%s/%s", std_dir, pkg);

    } else {
        fprintf(stderr, "barite: unknown source '%s' (available: local, cloud)\n", source);
        return 1;
    }

    if (!dir_exists(src_path)) {
        fprintf(stderr, "barite: package '%s' not found in %s\n", pkg, src_path);
        return 1;
    }

    // create destination and copy
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", dst_path);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "cp -r \"%s/\"* \"%s/\" 2>/dev/null", src_path, dst_path);
    system(cmd);

    // read and print package info
    char meta[512];
    char version[64]      = "?";
    char description[256] = "";
    snprintf(meta, sizeof(meta), "%s/package.barite", src_path);
    read_field(meta, "version", version, sizeof(version));
    read_description(meta, description, sizeof(description));

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
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';

        char meta[600];
        snprintf(meta, sizeof(meta), "%s/package.barite", line);

        char name[64]    = "";
        char version[64] = "?";
        char desc[256]   = "";
        read_field(meta, "name",    name,    sizeof(name));
        read_field(meta, "version", version, sizeof(version));
        read_description(meta, desc, sizeof(desc));

        // fallback if name field is missing
        if (name[0] == '\0') {
            char *slash = strrchr(line, '/');
            strncpy(name, slash ? slash + 1 : line, sizeof(name) - 1);
        }

        printf("  %-12s v%-8s %s\n", name, version, desc);
        found = 1;
    }
    pclose(pipe);

    if (!found) printf("  (none)\n");
}

// Info about barite
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
        // "installed" or anything else: look in std dir
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
        if (strcmp(fields[i], "description") == 0) {
            read_description(meta, val, sizeof(val));
            if (val[0]) printf("  %-12s %s\n", fields[i], val);
        } else {
            if (read_field(meta, fields[i], val, sizeof(val)))
                printf("  %-12s %s\n", fields[i], val);
        }
    }
}

// Search packages by name or description
void barite_search(const char *term, int include_cloud) {

    char std_dir[512];
    resolve_std_dir(std_dir, sizeof(std_dir));

    // pre-lowercase the search term once
    char term_lower[128];
    strncpy(term_lower, term, sizeof(term_lower) - 1);
    term_lower[sizeof(term_lower) - 1] = '\0';
    str_tolower(term_lower);

    int found = 0;

    // --- Search installed packages ---
    char find_cmd[600];
    snprintf(find_cmd, sizeof(find_cmd),
        "find \"%s\" -maxdepth 1 -mindepth 1 -type d 2>/dev/null", std_dir);

    FILE *pipe = popen(find_cmd, "r");
    if (pipe) {
        char line[512];
        int header_printed = 0;
        while (fgets(line, sizeof(line), pipe)) {
            int len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';

            char meta[600];
            snprintf(meta, sizeof(meta), "%s/package.barite", line);

            char name[64]    = "";
            char version[64] = "?";
            char desc[256]   = "";
            read_field(meta, "name",    name,    sizeof(name));
            read_field(meta, "version", version, sizeof(version));
            read_description(meta, desc, sizeof(desc));

            // fallback: use directory name if name field is missing
            if (name[0] == '\0') {
                char *slash = strrchr(line, '/');
                strncpy(name, slash ? slash + 1 : line, sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0';
            }

            // match against name, description, AND directory name
            char *dirslash = strrchr(line, '/');
            const char *dirname = dirslash ? dirslash + 1 : line;

            if (icontains(name,    term_lower) ||
                icontains(desc,    term_lower) ||
                icontains(dirname, term_lower))
            {
                if (!header_printed) {
                    printf("Installed packages matching '%s':\n", term);
                    header_printed = 1;
                }
                printf("  [installed] %-12s v%-8s %s\n", name, version, desc);
                found++;
            }
        }
        pclose(pipe);
    }

    // --- Search cloud packages (optional) ---
    if (include_cloud) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", CLOUD_CACHE_DIR);
        system(cmd);
        snprintf(cmd, sizeof(cmd),
            "git clone --depth 1 \"%s\" \"%s\" >/dev/null 2>&1",
            CLOUD_REPO_URL, CLOUD_CACHE_DIR);

        if (system(cmd) != 0) {
            fprintf(stderr, "barite: failed to fetch cloud packages for search\n"
                            "  check internet connection and git availability\n");
        } else {
            // try both root-level and std/ subdirectory layouts
            char root_std[512];
            snprintf(root_std, sizeof(root_std), "%s/std", CLOUD_CACHE_DIR);
            const char *roots[3] = { CLOUD_CACHE_DIR, root_std, NULL };

            int cloud_header_printed = 0;

            for (int r = 0; roots[r]; r++) {
                snprintf(find_cmd, sizeof(find_cmd),
                    "find \"%s\" -maxdepth 1 -mindepth 1 -type d 2>/dev/null", roots[r]);
                FILE *cp = popen(find_cmd, "r");
                if (!cp) continue;

                char line[512];
                while (fgets(line, sizeof(line), cp)) {
                    int len = strlen(line);
                    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';

                    // skip the cache dir itself appearing as a result
                    if (strcmp(line, CLOUD_CACHE_DIR) == 0) continue;

                    char meta[600];
                    snprintf(meta, sizeof(meta), "%s/package.barite", line);

                    char name[64]    = "";
                    char version[64] = "?";
                    char desc[256]   = "";
                    read_field(meta, "name",    name,    sizeof(name));
                    read_field(meta, "version", version, sizeof(version));
                    read_description(meta, desc, sizeof(desc));

                    if (name[0] == '\0') {
                        char *slash = strrchr(line, '/');
                        strncpy(name, slash ? slash + 1 : line, sizeof(name) - 1);
                        name[sizeof(name) - 1] = '\0';
                    }

                    char *dirslash = strrchr(line, '/');
                    const char *dirname = dirslash ? dirslash + 1 : line;

                    if (icontains(name,    term_lower) ||
                        icontains(desc,    term_lower) ||
                        icontains(dirname, term_lower))
                    {
                        if (!cloud_header_printed) {
                            if (found > 0) printf("\n");
                            printf("Cloud packages matching '%s':\n", term);
                            cloud_header_printed = 1;
                        }
                        printf("  [cloud]     %-12s v%-8s %s\n", name, version, desc);
                        found++;
                    }
                }
                pclose(cp);
            }

            snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", CLOUD_CACHE_DIR);
            (void)system(cmd);
        }
    }

    if (!found) {
        printf("No packages found matching '%s'", term);
        if (!include_cloud) printf(" (tip: use --cloud to also search remote packages)");
        printf("\n");
    }
}

// The help command of Barite

static void usage(void) {
    printf(
        "barite — CCPL package manager\n"
        "\n"
        "Usage:\n"
        "  barite-cli install <pkg>              install a cloud package (default source)\n"
        "  barite-cli install cloud <pkg>        install a cloud package\n"
        "  barite-cli install local <pkg>        install a local package\n"
        "  barite-cli remove <pkg>               remove an installed package\n"
        "  barite-cli list                       list installed packages\n"
        "  barite-cli search <term>              search installed packages\n"
        "  barite-cli search --cloud <term>      search installed + cloud packages\n"
        "  barite-cli info local <pkg>           show local package info\n"
        "  barite-cli info installed <pkg>       show info for installed package\n"
        "  barrite-cli --version, -v             show version information\n"
        "\n"
        "Examples:\n"
        "  barite-cli install math\n"
        "  barite-cli install cloud io\n"
        "  barite-cli install local math\n"
        "  barite-cli install local io\n"
        "  barite-cli install local shell\n"
        "  barite-cli list\n"
        "  barite-cli remove math\n"
        "  barite-cli search math\n"
        "  barite-cli search --cloud io\n"
        "\n"
        "Environment:\n"
        "  BARITE_STD_DIR   override the directory where packages are installed\n"
    );
}

// Main function, aka the command line interface (cli)

int main(int argc, char **argv) {

    if (argc < 2) {
        usage();
        return 0;
    }

    // Version commands, thanks for the idea!
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printf("Barite version %s\n", BARITE_VERSION);
        return 0;
    }

    const char *cmd = argv[1];

    // install [cloud|local] <pkg> [pkg2 ...]
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

    // remove <pkg> [pkg2 ...]
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

    // list
    if (strcmp(cmd, "list") == 0) {
        barite_list();
        return 0;
    }

    // search [--cloud] <term>
    if (strcmp(cmd, "search") == 0) {
        if (argc < 3) {
            fprintf(stderr, "usage: barite search [--cloud] <term>\n");
            return 1;
        }
        int cloud = 0;
        const char *term = argv[2];
        if (strcmp(argv[2], "--cloud") == 0) {
            cloud = 1;
            if (argc < 4) {
                fprintf(stderr, "usage: barite search --cloud <term>\n");
                return 1;
            }
            term = argv[3];
        }
        barite_search(term, cloud);
        return 0;
    }

    // info <source> <pkg>
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