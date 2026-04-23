// The io package runtime
static int ccpl_io_len(const char *s) { return s ? (int)strlen(s) : 0; }
static void ccpl_io_print_int(int v) { printf("%d\n", v); }
static void ccpl_io_print_float(double v) { printf("%.6g\n", v); }
static void ccpl_io_print_str(const char *s) { printf("%s\n", s ? s : ""); }

// string helper for the parser
static char _ccpl_concat_buf[4096];
static const char* ccpl_strcat_fn(const char *a, const char *b) {
    int _la = (int)strlen(a);
    int _lb = (int)strlen(b);
    if (_la + _lb < 4094) {
        memcpy(_ccpl_concat_buf, a, _la);
        memcpy(_ccpl_concat_buf + _la, b, _lb);
        _ccpl_concat_buf[_la + _lb] = 0;
    }
    return _ccpl_concat_buf;
}
