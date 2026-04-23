// bit package runtime
static int ccpl_bit_and(int a, int b) { return a & b; }
static int ccpl_bit_or(int a, int b) { return a | b; }
static int ccpl_bit_xor(int a, int b) { return a ^ b; }
static int ccpl_bit_shl(int a, int n) { return a << n; }
static int ccpl_bit_shr(int a, int n) { return a >> n; }
static int ccpl_bit_not(int a) { return ~a; }
static int ccpl_bit_set(int a, int pos) { return a | (1 << pos); }
static int ccpl_bit_clear(int a, int pos) { return a & ~(1 << pos); }
static int ccpl_bit_toggle(int a, int pos) { return a ^ (1 << pos); }
static int ccpl_bit_has(int a, int pos) { return (a & (1 << pos)) != 0; }
static int ccpl_bit_popcount(int a) {
	unsigned int x = (unsigned int)a;
	int c = 0;
	while (x) {
		c += (x & 1u);
		x >>= 1u;
	}
	return c;
}
