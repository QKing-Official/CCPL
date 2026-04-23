// cnum package runtime
static double ccpl_cnum_abs(double x) { return x < 0 ? -x : x; }
static double ccpl_cnum_min(double a, double b) { return a < b ? a : b; }
static double ccpl_cnum_max(double a, double b) { return a > b ? a : b; }
static double ccpl_cnum_clamp(double x, double lo, double hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}
static double ccpl_cnum_floor(double x) { return floor(x); }
static double ccpl_cnum_ceil(double x) { return ceil(x); }
static double ccpl_cnum_sqrt(double x) { return sqrt(x); }
static double ccpl_cnum_round(double x) { return round(x); }
static double ccpl_cnum_sign(double x) {
    if (x > 0) return 1.0;
    if (x < 0) return -1.0;
    return 0.0;
}
static int ccpl_cnum_is_even(int x) { return (x % 2) == 0; }
static int ccpl_cnum_gcd(int a, int b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        int t = b;
        b = a % b;
        a = t;
    }
    return a;
}
static int ccpl_cnum_lcm(int a, int b) {
    int g = ccpl_cnum_gcd(a, b);
    if (g == 0) return 0;
    return (a / g) * b;
}
