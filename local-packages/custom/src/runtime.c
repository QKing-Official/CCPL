// custom package runtime
// This is to show how you make own packages that get detected in the transpiler
static double ccpl_custom_inc(double x) { return x + 1.0; }
static double ccpl_custom_square(double x) { return x * x; }
static double ccpl_custom_cube(double x) { return x * x * x; }
static double ccpl_custom_avg(double a, double b) { return (a + b) / 2.0; }
static int ccpl_custom_factorial(int n) {
	if (n < 0) return 0;
	int r = 1;
	for (int i = 2; i <= n; i++) r *= i;
	return r;
}