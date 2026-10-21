/* freestanding aarch64 Linux: compare a 16 GiB image with a 4K CRC32 map.
 * usage: blkdiff IMAGE CRC DIFF IDX TOTAL_MB
 * No libc; only openat/read/write/close/exit syscalls. */
typedef unsigned long u64;
typedef unsigned int u32;
typedef unsigned char u8;

#define SYS_openat 56
#define SYS_read   63
#define SYS_write  64
#define SYS_close  57
#define SYS_exit   93
#define AT_FDCWD   (-100)
#define O_RDONLY   0
#define O_WRONLY   1
#define O_CREAT    64
#define O_TRUNC    512

static inline long sc(long n, long a, long b, long c, long d) {
	register long x8 asm("x8") = n;
	register long x0 asm("x0") = a;
	register long x1 asm("x1") = b;
	register long x2 asm("x2") = c;
	register long x3 asm("x3") = d;
	asm volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory");
	return x0;
}
static long xopen(const char *p, long fl) { return sc(SYS_openat, AT_FDCWD, (long)p, fl, 0644); }
static long xread(long fd, void *b, long n) { return sc(SYS_read, fd, (long)b, n, 0); }
static long xwrite(long fd, const void *b, long n) { return sc(SYS_write, fd, (long)b, n, 0); }
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
static void putnum(u64 v) {
	char b[24]; int i = 0;
	if (!v) { xwrite(1, "0", 1); return; }
	while (v) { b[i++] = '0' + (v % 10); v /= 10; }
	while (i) xwrite(1, &b[--i], 1);
}

static u32 table[256];
static u8 img[1 << 20];
static u8 crcbuf[1024];

static void crc_init(void) {
	for (u32 i = 0; i < 256; i++) {
		u32 c = i;
		for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
		table[i] = c;
	}
}
static u32 crc32(const u8 *p, u32 n) {
	u32 c = 0xFFFFFFFFu;
	for (u32 i = 0; i < n; i++) c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFFu;
}

__asm__(".global _start\n"
        "_start:\n"
        "mov x0, sp\n"
        "b start_c\n");

void start_c(long *sp) {
	long argc = sp[0];
	char **argv = (char **)(sp + 1);
	if (argc < 5) { put("usage: blkdiff IMAGE CRC DIFF IDX [TOTAL_MB]\n"); xexit(2); }
	u64 total_mb = argc >= 6 ? 0 : 16384;
	if (argc >= 6) { char *s = argv[5]; while (*s) total_mb = total_mb * 10 + (*s++ - '0'); }

	crc_init();
	long fi = xopen(argv[1], O_RDONLY);
	long fc = xopen(argv[2], O_RDONLY);
	long fd = xopen(argv[3], O_WRONLY | O_CREAT | O_TRUNC);
	long fx = xopen(argv[4], O_WRONLY | O_CREAT | O_TRUNC);
	if (fi < 0 || fc < 0 || fd < 0 || fx < 0) { put("open failed\n"); xexit(3); }

	u64 changed = 0;
	u32 idxbuf[256];
	for (u64 mb = 0; mb < total_mb; mb++) {
		if (read_full(fi, img, sizeof(img)) != sizeof(img)) { put("short image\n"); xexit(4); }
		if (read_full(fc, crcbuf, sizeof(crcbuf)) != sizeof(crcbuf)) { put("short crc\n"); xexit(5); }
		int nout = 0;
		for (u32 b = 0; b < 256; b++) {
			u32 want = crcbuf[b * 4] | (crcbuf[b * 4 + 1] << 8) |
			    (crcbuf[b * 4 + 2] << 16) | (crcbuf[b * 4 + 3] << 24);
			if (crc32(img + b * 4096, 4096) != want) {
				xwrite(fd, img + b * 4096, 4096);
				idxbuf[nout++] = (u32)(mb * 256 + b);
				changed++;
			}
		}
		if (nout) xwrite(fx, idxbuf, nout * 4);
		if ((mb & 1023) == 1023) {
			put("scanned "); putnum((mb + 1) >> 10); put(" GiB changed "); putnum(changed); put("\n");
		}
	}
	xclose(fi); xclose(fc); xclose(fd); xclose(fx);
	put("DONE changed "); putnum(changed); put("\n");
	xexit(0);
}
