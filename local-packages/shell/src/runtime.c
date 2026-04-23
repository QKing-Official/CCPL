// shell package runtime
static char _ccpl_shell_buf[4096];

static const char* ccpl_shell(const char *cmd) {
    FILE *p = popen(cmd, "r");
    if (!p) return "";
    int n = (int)fread(_ccpl_shell_buf, 1, sizeof(_ccpl_shell_buf) - 1, p);
    pclose(p);
    _ccpl_shell_buf[n] = '\0';
    if (n > 0 && _ccpl_shell_buf[n - 1] == '\n') _ccpl_shell_buf[n - 1] = '\0';
    return _ccpl_shell_buf;
}

static void ccpl_shell_exec(const char *cmd) {
    (void)system(cmd);
}
