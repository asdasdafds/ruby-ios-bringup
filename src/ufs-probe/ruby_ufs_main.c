#include <stdint.h>
#include "console.h"
#include "ufs.h"

#define rsr(r) ({ uint64_t _v; __asm__ volatile("mrs %0, " r : "=r"(_v)); _v; })

static inline void wdt_feed(void) { *(volatile uint32_t *)0x10007008UL = 1; }

volatile uint64_t sec_entry[8];
volatile uint64_t sec_any;

void exc_handler(uint64_t slot, uint64_t *regs)
{
	con_clear(BLUE);
	con_hex2("PANIC", "slot=", (uint32_t)slot, "cpu=", (uint32_t)rsr("mpidr_el1"));
	con_hex("ESR", rsr("esr_el1"));
	con_hex("ELR", rsr("elr_el1"));
	con_hex("FAR", rsr("far_el1"));
	con_hex("X0", regs[0]);
	con_hex("X1", regs[1]);
	for (;;) {
		wdt_feed();
		__asm__ volatile("wfe");
	}
}

void secondary_main(uint64_t idx)
{
	(void)idx;
	for (;;)
		__asm__ volatile("wfe");
}

void shim_main(uint64_t dtb, uint64_t sctlr)
{
	(void)dtb;
	con_clear(BLACK);
	con_str("RUBY", "UFS read-only probe");
	con_hex2("SCTLR", "was=", (uint32_t)sctlr, "M/C/I=", (uint32_t)(sctlr & 0x1005));
	con_hex2("MIDR", "midr=", (uint32_t)rsr("midr_el1"), "mpidr=", (uint32_t)rsr("mpidr_el1"));
	con_skip();

	test_ufs();

	con_skip();
	con_str("DONE", "UFS probe complete; power off");
	for (;;) {
		wdt_feed();
		__asm__ volatile("wfe");
	}
}
