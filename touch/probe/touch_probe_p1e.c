#include <stdint.h>
#include "console.h"

#define SPI3_BASE   0x11013000UL
#define TOP_BASE    0x10000000UL
#define IFRAO_BASE  0x10001000UL
#define GPIO_BASE   0x10005000UL

#define SCL_BIT     (1U << 4)   /* GPIO132 */
#define SDA_BIT     (1U << 5)   /* GPIO133 */

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

static uint8_t i2c_read_byte(void)
{
    uint8_t b = 0;
    int i;
    for (i = 7; i >= 0; i--) {
        if (i2c_read_bit())
            b |= 1U << i;
    }
    sda_release();
    delay_us(5);
    scl_release();
    delay_us(5);
    scl_low();
    delay_us(5);
    return b;
}

static int i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val)
{
    i2c_start();
    if (!i2c_write_byte(addr << 1)) goto fail;
    if (!i2c_write_byte(reg)) goto fail;
    if (!i2c_write_byte(val)) goto fail;
    i2c_stop();
    return 0;
fail:
    i2c_stop();
    return -1;
}

static int i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *val)
{
    i2c_start();
    if (!i2c_write_byte(addr << 1)) goto fail;
    if (!i2c_write_byte(reg)) goto fail;
    i2c_start();
    if (!i2c_write_byte((addr << 1) | 1)) goto fail;
    *val = i2c_read_byte();
    i2c_stop();
    return 0;
fail:
    i2c_stop();
    return -1;
}

static void i2c_pins_init(void)
{
    uint32_t v;
    /* pins 132/133 -> function 0 (GPIO) */
    v = rd32(GPIO_BASE + 0x400);
    wr32(GPIO_BASE + 0x400, v & ~(0xffU << 16));
    /* release both lines (input, external pull-up), drive data low */
    wr32(GPIO_BASE + 0x140, rd32(GPIO_BASE + 0x140) & ~(SCL_BIT | SDA_BIT));
    wr32(GPIO_BASE + 0x040, rd32(GPIO_BASE + 0x040) & ~(SCL_BIT | SDA_BIT));
    delay_us(20);
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

static void mk_read(uint8_t *t, uint32_t addr)
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
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

void shim_main(uint64_t dtb, uint64_t sctlr)
{
    uint8_t tx[32], rx[32];
    uint32_t st, v, di;
    int i, ok;
    uint8_t old3 = 0, new3 = 0, old2 = 0, new2 = 0;

    (void)dtb;
    con_clear(BLACK);
    con_str("RUBY", "TOUCH P1E I2C RMW");
    con_hex2("SCTLR", "was=", (uint32_t)sctlr, "midr=", (uint32_t)rd_midr());
    con_hex2("PRE", "5440=", rd32(GPIO_BASE + 0x440),
             "sta=", rd32(IFRAO_BASE + 0xac));

    /* SPI3 pins 160-163 function 2, gate CLR open, mux parent 0 */
    v = rd32(GPIO_BASE + 0x440);
    wr32(GPIO_BASE + 0x440, (v & ~0xffffU) | 0x2222U);
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

    if (i2c_read_reg(0x64, 0x05, &old3) != 0) {
        con_str("LDO3", "READ FAIL; abort");
        halt();
    }
    con_hex2("LDO3", "old=", old3, "bit6=", (old3 >> 6) & 1U);
    if (!(old3 & 0x40)) {
        if (i2c_write_reg(0x64, 0x05, old3 | 0x40) != 0) {
            con_str("LDO3", "WRITE FAIL; abort");
            halt();
        }
        if (i2c_read_reg(0x64, 0x05, &new3) != 0) {
            con_str("LDO3", "READBACK FAIL");
            halt();
        }
        con_hex2("LDO3", "new=", new3, "bit6=", (new3 >> 6) & 1U);
    } else {
        con_str("LDO3", "already on");
    }

    if (i2c_read_reg(0x64, 0x11, &old2) == 0) {
        con_hex2("LDO2", "old=", old2, "bit6=", (old2 >> 6) & 1U);
        if (!(old2 & 0x40)) {
            if (i2c_write_reg(0x64, 0x11, old2 | 0x40) == 0 &&
                i2c_read_reg(0x64, 0x11, &new2) == 0)
                con_hex2("LDO2", "new=", new2, "bit6=", (new2 >> 6) & 1U);
            else
                con_str("LDO2", "WRITE/READBACK FAIL");
        }
    } else {
        con_str("LDO2", "READ FAIL (vtp)");
    }

    delay_ms(30);

    /* reset 15: low 10 ms, high 100 ms */
    wr32(GPIO_BASE + 0x000, rd32(GPIO_BASE + 0x000) | (1U << 15));
    wr32(GPIO_BASE + 0x100, rd32(GPIO_BASE + 0x100) & ~(1U << 15));
    delay_ms(10);
    wr32(GPIO_BASE + 0x100, rd32(GPIO_BASE + 0x100) | (1U << 15));
    delay_ms(100);

    /* SPI3 PIO: div=64, mode 0, 8-bit, MSB first, CS0 */
    wr32(SPI3_BASE + 0x18, 1U << 2);
    wr32(SPI3_BASE + 0x18, 0);
    wr32(SPI3_BASE + 0x28, 0x001f001f);
    wr32(SPI3_BASE + 0x00, 0x003f003f);
    wr32(SPI3_BASE + 0x24, 0);
    wr32(SPI3_BASE + 0x18, 0x00033000);

    con_hex2("IOVDD", "dir=", rd32(GPIO_BASE + 0x030),
             "do=", rd32(GPIO_BASE + 0x130));
    con_hex2("SPI", "cfg0=", rd32(SPI3_BASE + 0x00),
             "cfg2=", rd32(SPI3_BASE + 0x28));
    con_hex2("SPI", "cmd=", rd32(SPI3_BASE + 0x18),
             "st0=", rd32(SPI3_BASE + 0x1c));
    con_hex2("GPIO", "do00=", rd32(GPIO_BASE + 0x100),
             "di00=", rd32(GPIO_BASE + 0x200));

    mk_read(tx, 0x10000);
    tx[0] = 0xf0;
    for (i = 0; i < 8; i++)
        tx[5 + i] = 0xaa;
    st = spi_xfer(tx, rx, 13);
    con_hex2("CONF W", "st=", st, "cmd=", rd32(SPI3_BASE + 0x18));
    delay_ms(5);

    mk_read(tx, 0x10000);
    st = spi_xfer(tx, rx, 16);
    ok = 1;
    for (i = 0; i < 8; i++)
        if (rx[8 + i] != 0xaa)
            ok = 0;
    con_hex2("CONF R", "0=", be32(&rx[8]), "1=", be32(&rx[12]));
    con_hex2("CONF", "st=", st, "ok=", ok ? 1U : 0U);

    mk_read(tx, 0x10014);
    st = spi_xfer(tx, rx, 16);
    con_hex2("VER", "0=", be32(&rx[8]), "1=", be32(&rx[12]));
    con_hex2("VER", "st=", st, "st0=", rd32(SPI3_BASE + 0x1c));

    con_str("DONE", "P1e I2C RMW; power off");
    halt();
}
