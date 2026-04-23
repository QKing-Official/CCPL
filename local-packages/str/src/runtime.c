// str package runtime
// For basic string manipulation

#include <ctype.h>

static int ccpl_str_contains(const char *s, const char *needle) {
    if (!s || !needle) return 0;
    return strstr(s, needle) != NULL;
}

static int ccpl_str_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return 0;
    size_t n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

static int ccpl_str_ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return 0;
    size_t ls = strlen(s), lx = strlen(suffix);
    if (lx > ls) return 0;
    return strcmp(s + (ls - lx), suffix) == 0;
}

static int ccpl_str_index_of(const char *s, const char *needle) {
    if (!s || !needle) return -1;
    char *p = strstr((char *)s, needle);
    if (!p) return -1;
    return (int)(p - s);
}

static int ccpl_str_to_int(const char *s) {
    if (!s) return 0;
    return (int)strtol(s, NULL, 10);
}

static double ccpl_str_to_float(const char *s) {
    if (!s) return 0.0;
    return strtod(s, NULL);
}

static char _ccpl_str_buf1[4096];
static char _ccpl_str_buf2[4096];
static char _ccpl_str_buf3[4096];

static const char *ccpl_str_trim(const char *s) {
    if (!s) return "";
    size_t n = strlen(s);
    size_t start = 0;
    while (start < n && isspace((unsigned char)s[start])) start++;
    size_t end = n;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;
    size_t len = end - start;
    if (len > sizeof(_ccpl_str_buf1) - 1) len = sizeof(_ccpl_str_buf1) - 1;
    memcpy(_ccpl_str_buf1, s + start, len);
    _ccpl_str_buf1[len] = '\0';
    return _ccpl_str_buf1;
}

static const char *ccpl_str_lower(const char *s) {
    if (!s) return "";
    size_t n = strlen(s);
    if (n > sizeof(_ccpl_str_buf2) - 1) n = sizeof(_ccpl_str_buf2) - 1;
    for (size_t i = 0; i < n; i++) _ccpl_str_buf2[i] = (char)tolower((unsigned char)s[i]);
    _ccpl_str_buf2[n] = '\0';
    return _ccpl_str_buf2;
}

static const char *ccpl_str_upper(const char *s) {
    if (!s) return "";
    size_t n = strlen(s);
    if (n > sizeof(_ccpl_str_buf3) - 1) n = sizeof(_ccpl_str_buf3) - 1;
    for (size_t i = 0; i < n; i++) _ccpl_str_buf3[i] = (char)toupper((unsigned char)s[i]);
    _ccpl_str_buf3[n] = '\0';
    return _ccpl_str_buf3;
}
