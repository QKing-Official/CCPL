// math package runtime
static double ccpl_math_add(double a, double b) { return a + b; }
static double ccpl_math_sub(double a, double b) { return a - b; }
static double ccpl_math_mul(double a, double b) { return a * b; }
static double ccpl_math_div(double a, double b) { return a / b; }
static int ccpl_math_mod(int a, int b) { return b ? (a % b) : 0; }
static double ccpl_math_pow(double a, double b) { return pow(a, b); }
