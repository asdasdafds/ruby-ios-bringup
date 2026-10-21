typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long u64;

#define FB_BASE 0x7ba10000UL
#define FB_BYTES 0x1df0000UL
#define FDT_MAGIC 0xd00dfeedU
#define MEM_BASE 0x40000000UL
#define MEM_SIZE 0x200000000UL
#define FB_NODE_BASE 0x7ba10000UL
#define FB_NODE_SIZE 0x1df0000UL
#define FDT_MAX 0x100000U

volatile u32 fault_stage = 0xFFFFFFFFU;

static u32 be32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static void fb_fill(u32 color)
{
    volatile u32 *p = (volatile u32 *)FB_BASE;
    u32 n = (u32)(FB_BYTES / 4);
    for (u32 i = 0; i < n; i++)
        p[i] = color;
    __asm__ volatile("dsb sy" ::: "memory");
}

static int str_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a++ != *b++)
            return 0;
    }
    return *a == 0 && *b == 0;
}

static int str_starts(const char *a, const char *b)
{
    while (*b) {
        if (*a++ != *b++)
            return 0;
    }
    return 1;
}

static int str_contains(const char *a, const char *b)
{
    for (; *a; a++) {
        const char *x = a;
        const char *y = b;
        while (*x && *y && *x == *y) {
            x++;
            y++;
        }
        if (!*y)
            return 1;
    }
    return 0;
}

static u32 align4(u32 n)
{
    return (n + 3U) & ~3U;
}

static int in_bounds(const u8 *p, u32 n, const u8 *end)
{
    return p <= end && (u64)(end - p) >= (u64)n;
}

static int cstr_ok(const char *s, const char *end)
{
    while (s < end) {
        if (!*s)
            return 1;
        s++;
    }
    return 0;
}

static void spin(void)
{
    volatile u32 *wdt = (volatile u32 *)0x10007008UL;
    for (;;) {
        *wdt = 1;
        for (volatile u32 i = 0; i < 0x200000U; i++)
            ;
    }
}

static void parse_fdt(u64 dtb, int *mem_ok, int *fb_ok)
{
    const u8 *base = (const u8 *)dtb;
    u32 totalsize = be32(base + 4);
    u32 off_struct = be32(base + 8);
    u32 off_strings = be32(base + 12);
    u32 size_strings = be32(base + 32);

    if (totalsize < 40U || totalsize > FDT_MAX)
        return;
    if (off_struct >= totalsize || off_strings >= totalsize)
        return;

    const u8 *end = base + totalsize;
    const u8 *p = base + off_struct;
    const char *strings = (const char *)(base + off_strings);
    const char *strings_end = (const char *)end;
    if (size_strings > totalsize - off_strings)
        size_strings = totalsize - off_strings;
    if (size_strings == 0U)
        return;

    fault_stage = 23;
    char node[64];
    node[0] = 0;

    u64 mbase = 0, msize = 0, fbase = 0, fsize = 0;
    int have_mem = 0, have_fb = 0;
    u32 guard = 0;

    fault_stage = 24;
    while (guard++ < 200000U) {
        if (!in_bounds(p, 4U, end))
            break;
        u32 tok = be32(p);
        p += 4;

        if (tok == 1U) {
            const char *name = (const char *)p;
            u32 max = (u32)(end - p);
            u32 len = 0;
            while (len < max && name[len])
                len++;
            if (len >= max)
                break;

            u32 copy = len < 63U ? len : 63U;
            for (u32 i = 0; i < copy; i++)
                node[i] = name[i];
            node[copy] = 0;

            u32 adv = align4(len + 1U);
            if (!in_bounds(p, adv, end))
                break;
            p += adv;
        } else if (tok == 2U) {
            node[0] = 0;
        } else if (tok == 3U) {
            if (!in_bounds(p, 8U, end))
                break;
            u32 len = be32(p);
            u32 nameoff = be32(p + 4);
            p += 8;

            if (nameoff >= size_strings)
                break;
            const char *pname = strings + nameoff;
            if (!cstr_ok(pname, strings_end))
                break;

            if (str_eq(pname, "reg") && len >= 16U && in_bounds(p, 16U, end)) {
                if (str_contains(node, "framebuffer")) {
                    fbase = ((u64)be32(p) << 32) | be32(p + 4);
                    fsize = ((u64)be32(p + 8) << 32) | be32(p + 12);
                    have_fb = 1;
                } else if (str_eq(node, "memory") || str_starts(node, "memory@")) {
                    mbase = ((u64)be32(p) << 32) | be32(p + 4);
                    msize = ((u64)be32(p + 8) << 32) | be32(p + 12);
                    have_mem = 1;
                }
            }

            u32 adv = align4(len);
            if (!in_bounds(p, adv, end))
                break;
            p += adv;
        } else if (tok == 4U) {
            continue;
        } else if (tok == 9U) {
            break;
        } else {
            break;
        }
    }

    fault_stage = 25;
    if (have_mem && mbase == (u64)MEM_BASE && msize == (u64)MEM_SIZE)
        *mem_ok = 1;
    if (have_fb && fbase == (u64)FB_NODE_BASE && fsize == (u64)FB_NODE_SIZE)
        *fb_ok = 1;
}

void shim_main(u64 dtb)
{
    fault_stage = 1;
    fb_fill(0xffff0000U);
    if (be32((const u8 *)dtb) != FDT_MAGIC)
        spin();

    fault_stage = 21;
    fb_fill(0xff00ff00U);
    fault_stage = 22;

    int mem_ok = 0;
    int fb_ok = 0;
    parse_fdt(dtb, &mem_ok, &fb_ok);

    if (mem_ok) {
        fault_stage = 3;
        fb_fill(0xff0000ffU);
    }
    if (fb_ok) {
        fault_stage = 4;
        fb_fill(0xffffffffU);
    }

    spin();
}
