/* freestanding aarch64 Linux: write diff blocks at their indices.
 * usage: blkpatch DEVICE DIFF IDX COUNT */
typedef unsigned int u32;
typedef unsigned char u8;
#define SYS_openat 56
#define SYS_read 63
#define SYS_write 64
#define SYS_close 57
#define SYS_lseek 62
#define SYS_exit 93
#define AT_FDCWD (-100)
#define O_RDONLY 0
#define O_RDWR 2

static inline long sc(long n, long a, long b, long c, long d) {
	register long x8 asm("x8") = n;
	register long x0 asm("x0") = a;
	register long x1 asm("x1") = b;
	register long x2 asm("x2") = c;
	register long x3 asm("x3") = d;
	asm volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory");
	return x0;
}
static long xopen(const char *p, long fl) { return sc(SYS_openat, AT_FDCWD, (long)p, fl, 0); }
static long xread(long fd, void *b, long n) { return sc(SYS_read, fd, (long)b, n, 0); }
static long xwrite(long fd, const void *b, long n) { return sc(SYS_write, fd, (long)b, n, 0); }
static long xseek(long fd, long off) { return sc(SYS_lseek, fd, off, 0, 0); }
static void xclose(long fd) { sc(SYS_close, fd, 0, 0, 0); }
static void xexit(long c) { sc(SYS_exit, c, 0, 0, 0); for (;;) {} }

static long read_full(long fd, void *b, long n) {
	long got = 0;
	while (got < n) {
		long r = xread(fd, (u8 *)b + got, n - got);
		if (r <= 0) return got;
		got += r;
	}
	return got;
}
static void put(const char *s) { long n = 0; while (s[n]) n++; xwrite(1, s, n); }
static void putnum(unsigned long v) {
	char b[24]; int i = 0;
	if (!v) { xwrite(1, "0", 1); return; }
	while (v) { b[i++] = '0' + (v % 10); v /= 10; }
	while (i) xwrite(1, &b[--i], 1);
}

__asm__(".global _start\n_start:\nmov x0, sp\nb start_c\n");

static u8 block[4096];
static u8 ib[4];

void start_c(long *sp) {
	long argc = sp[0];
	char **argv = (char **)(sp + 1);
	if (argc < 5) { put("usage: blkpatch DEV DIFF IDX COUNT\n"); xexit(2); }
	unsigned long count = 0;
	for (char *s = argv[4]; *s; s++) count = count * 10 + (*s - '0');
	long dev = xopen(argv[1], O_RDWR);
	long fd = xopen(argv[2], O_RDONLY);
	long fi = xopen(argv[3], O_RDONLY);
	if (dev < 0 || fd < 0 || fi < 0) { put("open failed\n"); xexit(3); }
	for (unsigned long j = 0; j < count; j++) {
		if (read_full(fi, ib, 4) != 4 || read_full(fd, block, 4096) != 4096) {
			put("short input at "); putnum(j); put("\n"); xexit(4);
		}
		u32 b = ib[0] | (ib[1] << 8) | (ib[2] << 16) | ((u32)ib[3] << 24);
		if (xseek(dev, (long)b * 4096) < 0 || xwrite(dev, block, 4096) != 4096) {
			put("write failed at "); putnum(j); put("\n"); xexit(5);
		}
		if ((j & 1023) == 1023) { put("wrote "); putnum(j + 1); put("\n"); }
	}
	put("DONE wrote "); putnum(count); put("\n");
	xexit(0);
}
