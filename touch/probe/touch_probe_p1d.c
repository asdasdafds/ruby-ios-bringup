#include <stdint.h>
#include "console.h"

#define SPI3_BASE   0x11013000UL
#define TOP_BASE    0x10000000UL
#define IFRAO_BASE  0x10001000UL
#define GPIO_BASE   0x10005000UL

static inline uint32_t rd32(uintptr_t a)
{
    return *(volatile uint32_t *)a;
}

static inline void wr32(uintptr_t a, uint32_t v)
{
    *(volatile uint32_t *)a = v;
}

static inline uint64_t rd_cntfrq(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v));
    return v;
}

static inline uint64_t rd_cntpct(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(v));
    return v;
}

static inline uint64_t rd_esr(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(v));
    return v;
}

static inline uint64_t rd_elr(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, elr_el1" : "=r"(v));
    return v;
}

static inline uint64_t rd_far(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, far_el1" : "=r"(v));
    return v;
}

static inline uint64_t rd_midr(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(v));
    return v;
}

static inline void wdt_feed(void)
{
    *(volatile uint32_t *)0x10007008UL = 1;
}

static void delay_ms(unsigned ms)
{
    uint64_t f = rd_cntfrq();
    uint64_t end = rd_cntpct() + (f / 1000) * ms;
    while (rd_cntpct() < end)
        wdt_feed();
}

volatile uint64_t sec_entry[8];
volatile uint64_t sec_any;

void secondary_main(uint64_t idx)
{
    (void)idx;
    for (;;)
        __asm__ volatile("wfe");
}

void exc_handler(uint64_t slot, uint64_t *regs)
{
    (void)regs;
    con_clear(BLUE);
    con_hex2("PANIC", "slot=", (uint32_t)slot, "esr=", (uint32_t)rd_esr());
    con_hex("ELR", rd_elr());
    con_hex("FAR", rd_far());
    for (;;) {
        wdt_feed();
        __asm__ volatile("wfe");
    }
}

static uint32_t spi_xfer(const uint8_t *tx, uint8_t *rx, unsigned len)
{
    uint32_t v, cmd, st;
    unsigned i, j;

    wr32(SPI3_BASE + 0x04, 0x3f | ((len - 1) << 16));
    for (i = 0; i < len; i += 4) {
        v = 0;
        for (j = 0; j < 4 && i + j < len; j++)
            v |= (uint32_t)tx[i + j] << (8 * j);
        wr32(SPI3_BASE + 0x10, v);
    }

    cmd = rd32(SPI3_BASE + 0x18) & ~1U;
    wr32(SPI3_BASE + 0x18, cmd);
    wr32(SPI3_BASE + 0x18, cmd | 1U);
    delay_ms(5);
    st = rd32(SPI3_BASE + 0x1c);

    for (i = 0; i < len; i += 4) {
        v = rd32(SPI3_BASE + 0x14);
        for (j = 0; j < 4 && i + j < len; j++)
            rx[i + j] = (uint8_t)(v >> (8 * j));
    }
    wr32(SPI3_BASE + 0x18, cmd);
    return st;
}

static void mk_read(uint8_t *t, uint32_t addr, unsigned n)
{
    unsigned i;
    for (i = 0; i < 32; i++)
        t[i] = 0;
    t[0] = 0xf1;
    t[1] = (addr >> 24) & 0xff;
    t[2] = (addr >> 16) & 0xff;
    t[3] = (addr >> 8) & 0xff;
    t[4] = addr & 0xff;
    t[5] = 0xff;
    t[6] = 0xff;
    t[7] = 0xff;
    (void)n;
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

void shim_main(uint64_t dtb, uint64_t sctlr)
{
    uint8_t tx[32], rx[32];
    uint32_t st, v;
    int i, ok;

    (void)dtb;
    con_clear(BLACK);
    con_str("RUBY", "TOUCH P1D IOVDD ON");
    con_hex2("SCTLR", "was=", (uint32_t)sctlr, "midr=", (uint32_t)rd_midr());
    con_hex2("PRE", "5440=", rd32(GPIO_BASE + 0x440),
             "sta=", rd32(IFRAO_BASE + 0xac));

    /* SPI3 pins 160-163: function 2 */
    v = rd32(GPIO_BASE + 0x440);
    wr32(GPIO_BASE + 0x440, (v & ~0xffffU) | 0x2222U);
    /* SPI3 gate: IFRAO2 is active-low; CLR bit 10 = ungated */
    wr32(IFRAO_BASE + 0xa8, 1U << 10);
    delay_ms(1);

    /* SPI mux -> parent 0 (tck_26m_mx9); clear CLK_CFG_6 bit 8 and commit */
    wr32(TOP_BASE + 0x78, 1U << 8);
    wr32(TOP_BASE + 0x04, 1U << 25);
    delay_ms(1);
    con_hex2("MUX", "cfg6=", rd32(TOP_BASE + 0x70),
             "upd=", rd32(TOP_BASE + 0x04));

    /* IOVDD switch pin 110: DIR bit14=1, DO bit14=1 */
    wr32(GPIO_BASE + 0x030, rd32(GPIO_BASE + 0x030) | (1U << 14));
    wr32(GPIO_BASE + 0x130, rd32(GPIO_BASE + 0x130) | (1U << 14));
    delay_ms(3);
    con_hex2("IOVDD", "dir=", rd32(GPIO_BASE + 0x030),
             "do=", rd32(GPIO_BASE + 0x130));

    /* reset pulse: reset 15 low 2 ms, high, settle 100 ms */
    wr32(GPIO_BASE + 0x000, rd32(GPIO_BASE + 0x000) | (1U << 15));
    wr32(GPIO_BASE + 0x100, rd32(GPIO_BASE + 0x100) & ~(1U << 15));
    delay_ms(2);
    wr32(GPIO_BASE + 0x100, rd32(GPIO_BASE + 0x100) | (1U << 15));
    delay_ms(100);

    /* SPI3 PIO: div=64, mode 0, 8-bit, MSB first, CS0 */
    wr32(SPI3_BASE + 0x18, 1U << 2);
    wr32(SPI3_BASE + 0x18, 0);
    wr32(SPI3_BASE + 0x28, 0x001f001f);
    wr32(SPI3_BASE + 0x00, 0x003f003f);
    wr32(SPI3_BASE + 0x24, 0);
    wr32(SPI3_BASE + 0x18, 0x00033000);

    con_hex2("POST", "5440=", rd32(GPIO_BASE + 0x440),
             "sta=", rd32(IFRAO_BASE + 0xac));
    con_hex2("SPI", "cfg0=", rd32(SPI3_BASE + 0x00),
             "cfg2=", rd32(SPI3_BASE + 0x28));
    con_hex2("SPI", "cmd=", rd32(SPI3_BASE + 0x18),
             "st0=", rd32(SPI3_BASE + 0x1c));
    con_hex2("GPIO", "do00=", rd32(GPIO_BASE + 0x100),
             "di00=", rd32(GPIO_BASE + 0x200));

    if (rd32(SPI3_BASE + 0x00) == 0 && rd32(SPI3_BASE + 0x18) == 0) {
        con_str("STOP", "SPI regs still zero");
        for (;;) {
            wdt_feed();
            __asm__ volatile("wfe");
        }
    }

    /* dev_confirm: write 8 x 0xAA to 0x10000, read back */
    mk_read(tx, 0x10000, 8);
    tx[0] = 0xf0;
    for (i = 0; i < 8; i++)
        tx[5 + i] = 0xaa;
    st = spi_xfer(tx, rx, 13);
    con_hex2("CONF W", "st=", st, "cmd=", rd32(SPI3_BASE + 0x18));
    delay_ms(5);

    mk_read(tx, 0x10000, 8);
    st = spi_xfer(tx, rx, 16);
    ok = 1;
    for (i = 0; i < 8; i++)
        if (rx[8 + i] != 0xaa)
            ok = 0;
    con_hex2("CONF R", "0=", be32(&rx[8]), "1=", be32(&rx[12]));
    con_hex2("CONF", "st=", st, "ok=", ok ? 1U : 0U);

    /* version first 8 bytes at 0x10014: expect "BERLIN" */
    mk_read(tx, 0x10014, 8);
    st = spi_xfer(tx, rx, 16);
    con_hex2("VER", "0=", be32(&rx[8]), "1=", be32(&rx[12]));
    con_hex2("VER", "st=", st, "st0=", rd32(SPI3_BASE + 0x1c));

    con_str("DONE", "P1 read/dev_confirm; power off");
    for (;;) {
        wdt_feed();
        __asm__ volatile("wfe");
    }
}
