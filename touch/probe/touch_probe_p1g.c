#include <stdint.h>
#include "console.h"

#define SPI3_BASE   0x11013000UL
#define TOP_BASE    0x10000000UL
#define IFRAO_BASE  0x10001000UL
#define GPIO_BASE   0x10005000UL

#define SCL_BIT     (1U << 4)   /* GPIO132 */
#define SDA_BIT     (1U << 5)   /* GPIO133 */

/* SPI3 bank 5: MO=160 MI=161 CSB=162 CLK=163 */
#define PIN_MO      (1U << 0)
#define PIN_MI      (1U << 1)
#define PIN_CS      (1U << 2)
#define PIN_CLK     (1U << 3)

static inline uint32_t rd32(uintptr_t a) { return *(volatile uint32_t *)a; }
static inline void wr32(uintptr_t a, uint32_t v) { *(volatile uint32_t *)a = v; }
static inline void wdt_feed(void) { *(volatile uint32_t *)0x10007008UL = 1; }

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

static void delay_us(unsigned us)
{
    uint64_t f = rd_cntfrq();
    uint64_t ticks = (f / 1000000) * us;
    uint64_t end = rd_cntpct() + (ticks ? ticks : 1);
    while (rd_cntpct() < end)
        wdt_feed();
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

static void halt(void)
{
    for (;;) {
        wdt_feed();
        __asm__ volatile("wfe");
    }
}

static void scl_release(void)
{
    wr32(GPIO_BASE + 0x040, rd32(GPIO_BASE + 0x040) & ~SCL_BIT);
}

static void scl_low(void)
{
    wr32(GPIO_BASE + 0x140, rd32(GPIO_BASE + 0x140) & ~SCL_BIT);
    wr32(GPIO_BASE + 0x040, rd32(GPIO_BASE + 0x040) | SCL_BIT);
}

static void sda_release(void)
{
    wr32(GPIO_BASE + 0x040, rd32(GPIO_BASE + 0x040) & ~SDA_BIT);
}

static void sda_low(void)
{
    wr32(GPIO_BASE + 0x140, rd32(GPIO_BASE + 0x140) & ~SDA_BIT);
    wr32(GPIO_BASE + 0x040, rd32(GPIO_BASE + 0x040) | SDA_BIT);
}

static int sda_read(void)
{
    sda_release();
    delay_us(2);
    return (rd32(GPIO_BASE + 0x240) & SDA_BIT) != 0;
}

static uint8_t crc8_msb(const uint8_t *p, int n)
{
    uint8_t crc = 0;
    int i, b;
    for (i = 0; i < n; i++) {
        crc ^= p[i];
        for (b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07)
                               : (uint8_t)(crc << 1);
    }
    return crc;
}

static void i2c_start(void)
{
    sda_release();
    scl_release();
    delay_us(5);
    sda_low();
    delay_us(5);
    scl_low();
    delay_us(5);
}

static void i2c_stop(void)
{
    sda_low();
    delay_us(5);
    scl_release();
    delay_us(5);
    sda_release();
    delay_us(5);
}

static void i2c_write_bit(int bit)
{
    if (bit)
        sda_release();
    else
        sda_low();
    delay_us(5);
    scl_release();
    delay_us(5);
    scl_low();
    delay_us(5);
}

static int i2c_read_bit(void)
{
    int bit;
    sda_release();
    delay_us(5);
    scl_release();
    delay_us(5);
    bit = sda_read();
    scl_low();
    delay_us(5);
    return bit;
}

static int i2c_write_byte(uint8_t b)
{
    int i, ack;
    for (i = 7; i >= 0; i--)
        i2c_write_bit((b >> i) & 1);
    sda_release();
    delay_us(5);
    scl_release();
    delay_us(5);
    ack = !sda_read();
    scl_low();
    delay_us(5);
    return ack;
}

static uint8_t i2c_read_byte_ack(int ack)
{
    uint8_t b = 0;
    int i;
    for (i = 7; i >= 0; i--) {
        if (i2c_read_bit())
            b |= 1U << i;
    }
    sda_release();
    delay_us(5);
    if (ack)
        sda_low();
    delay_us(5);
    scl_release();
    delay_us(5);
    scl_low();
    delay_us(5);
    sda_release();
    return b;
}

static int mt6360_write_reg(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t c[3];
    uint8_t crc;
    c[0] = addr << 1;
    c[1] = reg & 0x3f;
    c[2] = val;
    crc = crc8_msb(c, 3);
    i2c_start();
    if (!i2c_write_byte(addr << 1)) goto fail;
    if (!i2c_write_byte(reg & 0x3f)) goto fail;
    if (!i2c_write_byte(val)) goto fail;
    if (!i2c_write_byte(crc)) goto fail;
    if (!i2c_write_byte(0)) goto fail;
    i2c_stop();
    return 0;
fail:
    i2c_stop();
    return -1;
}

static int mt6360_read_reg(uint8_t addr, uint8_t reg, uint8_t *val)
{
    uint8_t d[2], c[3];
    i2c_start();
    if (!i2c_write_byte(addr << 1)) goto fail;
    if (!i2c_write_byte(reg & 0x3f)) goto fail;
    i2c_start();
    if (!i2c_write_byte((addr << 1) | 1)) goto fail;
    d[0] = i2c_read_byte_ack(1);
    d[1] = i2c_read_byte_ack(0);
    i2c_stop();
    c[0] = (addr << 1) | 1;
    c[1] = reg & 0x3f;
    c[2] = d[0];
    if (crc8_msb(c, 3) != d[1])
        return -1;
    *val = d[0];
    return 0;
fail:
    i2c_stop();
    return -1;
}

static void i2c_pins_init(void)
{
    uint32_t v;
    v = rd32(GPIO_BASE + 0x400);
    wr32(GPIO_BASE + 0x400, v & ~(0xffU << 16));
    wr32(GPIO_BASE + 0x140, rd32(GPIO_BASE + 0x140) & ~(SCL_BIT | SDA_BIT));
    wr32(GPIO_BASE + 0x040, rd32(GPIO_BASE + 0x040) & ~(SCL_BIT | SDA_BIT));
    delay_us(20);
}

/* ---- GPIO bit-bang SPI, Mode 0, CS active low ---- */

static unsigned g_half_us = 2;

static void bb_set_do(uint32_t mask, int high)
{
    uint32_t v = rd32(GPIO_BASE + 0x150);
    if (high)
        v |= mask;
    else
        v &= ~mask;
    wr32(GPIO_BASE + 0x150, v);
}

static void bb_pins_init(void)
{
    uint32_t v;
    /* pins 160-163 -> function 0 (GPIO) */
    v = rd32(GPIO_BASE + 0x440);
    wr32(GPIO_BASE + 0x440, v & ~0xffffU);
    /* MO/CS/CLK output, MI input */
    v = rd32(GPIO_BASE + 0x050);
    wr32(GPIO_BASE + 0x050,
         (v & ~0x0fU) | (PIN_MO | PIN_CS | PIN_CLK));
    /* idle: CS high, CLK low, MO low */
    v = rd32(GPIO_BASE + 0x150);
    wr32(GPIO_BASE + 0x150, (v & ~0x0fU) | PIN_CS);
    delay_us(20);
}

static void bb_cs(int high)
{
    bb_set_do(PIN_CS, high);
    delay_us(g_half_us);
}

static int bb_mi(void)
{
    return (rd32(GPIO_BASE + 0x250) & PIN_MI) != 0;
}

static void bb_xfer(const uint8_t *tx, uint8_t *rx, unsigned len)
{
    unsigned i;
    int b;

    bb_cs(0);
    for (i = 0; i < len; i++) {
        uint8_t r = 0;
        for (b = 7; b >= 0; b--) {
            bb_set_do(PIN_MO, (tx[i] >> b) & 1);
            delay_us(g_half_us);
            bb_set_do(PIN_CLK, 1);
            delay_us(g_half_us);
            if (rx)
                r |= (uint8_t)(bb_mi() << b);
            bb_set_do(PIN_CLK, 0);
            delay_us(g_half_us);
        }
        if (rx)
            rx[i] = r;
    }
    bb_cs(1);
}

static void mk_read(uint8_t *t, uint32_t addr, unsigned total)
{
    unsigned i;
    for (i = 0; i < total; i++)
        t[i] = 0;
    t[0] = 0xf1;
    t[1] = (addr >> 24) & 0xff;
    t[2] = (addr >> 16) & 0xff;
    t[3] = (addr >> 8) & 0xff;
    t[4] = addr & 0xff;
    t[5] = 0xff;
    t[6] = 0xff;
    t[7] = 0xff;
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t be64(const uint8_t *p)
{
    return ((uint64_t)be32(p) << 32) | be32(p + 4);
}

static int bb_dev_confirm(uint8_t *last, unsigned *used_half)
{
    uint8_t tx[16], rx[16];
    int attempt, i, ok;

    for (attempt = 0; attempt < 2; attempt++) {
        g_half_us = attempt ? 10 : 2;
        tx[0] = 0xf0;
        tx[1] = 0x00;
        tx[2] = 0x01;
        tx[3] = 0x00;
        tx[4] = 0x00;
        for (i = 0; i < 8; i++)
            tx[5 + i] = 0xaa;
        bb_xfer(tx, 0, 13);
        delay_ms(5);

        mk_read(tx, 0x10000, 16);
        bb_xfer(tx, rx, 16);
        ok = 1;
        for (i = 0; i < 8; i++)
            if (rx[8 + i] != 0xaa)
                ok = 0;
        for (i = 0; i < 8; i++)
            last[i] = rx[8 + i];
        if (ok) {
            *used_half = g_half_us;
            return 0;
        }
    }
    return -1;
}

static int ver_ok(const uint8_t *b)
{
    uint32_t sum = 0;
    int i;
    for (i = 0; i < 26; i++)
        sum += b[i];
    return (sum & 0xffff) == (uint32_t)(b[26] | (b[27] << 8));
}

void shim_main(uint64_t dtb, uint64_t sctlr)
{
    uint8_t tx[40], rx[40];
    uint32_t v, di;
    int i;
    uint8_t old3 = 0, new3 = 0, old2 = 0, new2 = 0;
    uint8_t last[8];
    unsigned used_half = 0;
    int confirm_ok;

    (void)dtb;
    con_clear(BLACK);
    con_str("RUBY", "TOUCH P1G BB-SPI");
    con_hex2("SCTLR", "was=", (uint32_t)sctlr, "midr=", (uint32_t)rd_midr());
    con_hex2("PRE", "5440=", rd32(GPIO_BASE + 0x440),
             "sta=", rd32(IFRAO_BASE + 0xac));

    /* keep SPI3 clock ungated in case firmware expects it */
    wr32(IFRAO_BASE + 0xa8, 1U << 10);
    wr32(TOP_BASE + 0x78, 1U << 8);
    wr32(TOP_BASE + 0x04, 1U << 25);
    delay_ms(1);

    /* IOVDD pin110 high, 3 ms */
    wr32(GPIO_BASE + 0x030, rd32(GPIO_BASE + 0x030) | (1U << 14));
    wr32(GPIO_BASE + 0x130, rd32(GPIO_BASE + 0x130) | (1U << 14));
    delay_ms(3);

    /* I2C5 bit-bang: pins 132/133 -> GPIO */
    i2c_pins_init();
    di = rd32(GPIO_BASE + 0x240);
    con_hex2("I2C", "scl=", (di & SCL_BIT) ? 1U : 0U,
             "sda=", (di & SDA_BIT) ? 1U : 0U);
    if ((di & (SCL_BIT | SDA_BIT)) != (SCL_BIT | SDA_BIT)) {
        con_str("I2C", "idle low; abort");
        halt();
    }

    if (mt6360_read_reg(0x64, 0x05, &old3) != 0) {
        con_str("LDO3", "READ FAIL; abort");
        halt();
    }
    con_hex2("LDO3", "old=", old3, "bit6=", (old3 >> 6) & 1U);
    if (!(old3 & 0x40)) {
        if (mt6360_write_reg(0x64, 0x05, old3 | 0x40) != 0) {
            con_str("LDO3", "WRITE FAIL; abort");
            halt();
        }
        if (mt6360_read_reg(0x64, 0x05, &new3) != 0) {
            con_str("LDO3", "READBACK FAIL");
            halt();
        }
        con_hex2("LDO3", "new=", new3, "bit6=", (new3 >> 6) & 1U);
    } else {
        con_str("LDO3", "already on");
    }

    if (mt6360_read_reg(0x64, 0x11, &old2) == 0) {
        con_hex2("LDO2", "old=", old2, "bit6=", (old2 >> 6) & 1U);
        if (!(old2 & 0x40)) {
            if (mt6360_write_reg(0x64, 0x11, old2 | 0x40) == 0 &&
                mt6360_read_reg(0x64, 0x11, &new2) == 0)
                con_hex2("LDO2", "new=", new2, "bit6=", (new2 >> 6) & 1U);
            else
                con_str("LDO2", "WRITE/READBACK FAIL");
        }
    } else {
        con_str("LDO2", "READ FAIL (vtp)");
    }

    delay_ms(30);

    /* SPI3 pins 160-163 -> GPIO, idle CS high / CLK low */
    bb_pins_init();
    con_hex2("BB", "mode=", rd32(GPIO_BASE + 0x440),
             "dir50=", rd32(GPIO_BASE + 0x050));
    con_hex2("BB", "do50=", rd32(GPIO_BASE + 0x150),
             "di50=", rd32(GPIO_BASE + 0x250));
    con_hex2("IRQ", "di200=", rd32(GPIO_BASE + 0x200),
             "bit14=", (rd32(GPIO_BASE + 0x200) >> 14) & 1U);

    /* DI/IES sanity: drive CS ourselves, read DI bit2 back */
    {
        uint32_t cs_lo, cs_hi;
        bb_cs(0);
        cs_lo = (rd32(GPIO_BASE + 0x250) >> 2) & 1U;
        bb_cs(1);
        cs_hi = (rd32(GPIO_BASE + 0x250) >> 2) & 1U;
        con_hex2("DI TEST", "cslo=", cs_lo, "cshi=", cs_hi);
    }

    /* reset 15: low 10 ms, high 100 ms */
    wr32(GPIO_BASE + 0x000, rd32(GPIO_BASE + 0x000) | (1U << 15));
    wr32(GPIO_BASE + 0x100, rd32(GPIO_BASE + 0x100) & ~(1U << 15));
    delay_ms(10);
    wr32(GPIO_BASE + 0x100, rd32(GPIO_BASE + 0x100) | (1U << 15));
    delay_ms(100);

    con_hex2("POST", "di50=", rd32(GPIO_BASE + 0x250),
             "irq=", (rd32(GPIO_BASE + 0x200) >> 14) & 1U);

    confirm_ok = bb_dev_confirm(last, &used_half);
    con_hex2("CONF", "ok=", confirm_ok == 0 ? 1U : 0U,
             "half=", used_half);
    con_hex2("CONF R", "0=", be32(last), "1=", be32(last + 4));

    /* version: 8-byte prefix + 28-byte struct at 0x10014 */
    g_half_us = confirm_ok == 0 ? 2 : 10;
    mk_read(tx, 0x10014, 36);
    bb_xfer(tx, rx, 36);
    con_hex2("VER CS", "ok=", (uint32_t)ver_ok(&rx[8]),
             "sum=", (uint32_t)(rx[34] | (rx[35] << 8)));
    con_hex("VER0", be64(&rx[8]));
    con_hex("VER1", be64(&rx[16]));
    con_hex("VER2", be64(&rx[24]));
    con_hex("VER3", be32(&rx[32]));

    if (confirm_ok == 0) {
        char rom[7];
        char pid[9];
        for (i = 0; i < 6; i++)
            rom[i] = (char)rx[8 + i];
        rom[6] = 0;
        for (i = 0; i < 8; i++)
            pid[i] = (char)rx[18 + i];
        pid[8] = 0;
        con_str("ROM", rom);
        con_str("PID", pid);
        con_hex2("SENSOR", "id=", rx[31], "vid0=", be32(&rx[26]));
    }

    con_str("DONE", "P1G bb-spi; power off");
    halt();
}
