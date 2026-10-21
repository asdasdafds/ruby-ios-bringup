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

static inline uint64_t mrs_esr(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(v));
    return v;
}

static inline uint64_t mrs_elr(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, elr_el1" : "=r"(v));
    return v;
}

static inline uint64_t mrs_far(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, far_el1" : "=r"(v));
    return v;
}

static inline uint64_t mrs_midr(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(v));
    return v;
}

static inline void wdt_feed(void)
{
    *(volatile uint32_t *)0x10007008UL = 1;
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
    con_hex2("PANIC", "slot=", (uint32_t)slot, "esr=", (uint32_t)mrs_esr());
    con_hex("ELR", mrs_elr());
    con_hex("FAR", mrs_far());
    con_hex("MIDR", mrs_midr());
    for (;;) {
        wdt_feed();
        __asm__ volatile("wfe");
    }
}

void shim_main(uint64_t dtb, uint64_t sctlr)
{
    (void)dtb;

    con_clear(BLACK);
    con_str("RUBY", "TOUCH P0 READ-ONLY");
    con_hex2("SCTLR", "was=", (uint32_t)sctlr, "midr=", (uint32_t)mrs_midr());
    con_str("NOTE", "no MMIO writes");
    con_skip();

    con_hex2("SPI3 CFG", "0=", rd32(SPI3_BASE + 0x00),
             "1=", rd32(SPI3_BASE + 0x04));
    con_hex2("SPI3 CMD", "cmd=", rd32(SPI3_BASE + 0x18),
             "st0=", rd32(SPI3_BASE + 0x1c));
    con_hex2("SPI3 PAD", "pad=", rd32(SPI3_BASE + 0x24),
             "cfg2=", rd32(SPI3_BASE + 0x28));
    con_hex2("TOP", "cfg6=", rd32(TOP_BASE + 0x70),
             "upd=", rd32(TOP_BASE + 0x04));
    con_hex2("IFRAO", "set=", rd32(IFRAO_BASE + 0xa4),
             "sta=", rd32(IFRAO_BASE + 0xac));
    con_hex2("PINMUX", "5440=", rd32(GPIO_BASE + 0x440),
             "5310=", rd32(GPIO_BASE + 0x310));
    con_hex2("PINMUX", "53d0=", rd32(GPIO_BASE + 0x3d0),
             "dir0=", rd32(GPIO_BASE + 0x000));
    con_hex2("GPIO", "dir30=", rd32(GPIO_BASE + 0x030),
             "do00=", rd32(GPIO_BASE + 0x100));
    con_hex2("GPIO", "do30=", rd32(GPIO_BASE + 0x130),
             "di00=", rd32(GPIO_BASE + 0x200));
    con_skip();
    con_str("DONE", "read-only snapshot; power off");
    for (;;) {
        wdt_feed();
        __asm__ volatile("wfe");
    }
}
