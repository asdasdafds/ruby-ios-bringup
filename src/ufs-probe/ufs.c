/*
 * Read-only UFS probe for ruby (MT6877). LK has already initialised the
 * controller/PHY to read boot_a, so reuse the running host: dump UFSHCI
 * registers, take over the transfer request list, and issue TUR / READ
 * CAPACITY / READ(10) on LUN 0..2. No SCSI WRITE exists here.
 */
#include <stdint.h>
#include "console.h"
#include "ufs.h"

#define UFS_BASE	0x11270000UL	/* ufshci@11270000, ruby live FDT */

#define REG_CAP		0x00
#define REG_VER		0x08
#define REG_IS		0x20
#define REG_HCS		0x30
#define REG_HCE		0x34
#define REG_UTRLBA	0x50
#define REG_UTRLBAU	0x54
#define REG_UTRLDBR	0x58
#define REG_UTRLCLR	0x5c
#define REG_UTRLRSR	0x60

#define HCS_DP		(1u << 0)
#define HCS_UTRLRDY	(1u << 1)

#define DATA_SIZE	32768u
#define BLOCK_DEFAULT	4096u

static inline uint32_t ufs_rd(uint32_t off) { return *(volatile uint32_t *)(UFS_BASE + off); }
static inline void ufs_wr(uint32_t off, uint32_t v) { *(volatile uint32_t *)(UFS_BASE + off) = v; }

struct utrd {
	uint32_t dw0;
	uint32_t dw1;
	uint32_t dw2;		/* [7:0] OCS */
	uint32_t dw3;
	uint32_t ucdba;
	uint32_t ucdbau;
	uint32_t resp;		/* [31:16] response offset, [15:0] length (DWORDs) */
	uint32_t prdt;		/* [31:16] PRDT offset, [15:0] entries */
};

struct ucd {
	uint8_t cmd[512];
	uint8_t resp[512];
	struct {
		uint32_t addr_lo, addr_hi, rsvd, dbc;
	} prdt[1];
};

static struct utrd utrl[32] __attribute__((aligned(1024)));
static struct ucd ucd __attribute__((aligned(128)));
static uint8_t data[DATA_SIZE] __attribute__((aligned(4096)));
static uint8_t capbuf[8] __attribute__((aligned(64)));

static uint64_t ticks(void)
{
	uint64_t t;
	__asm__ volatile("isb; mrs %0, cntvct_el0" : "=r"(t));
	return t;
}

static uint64_t freq(void)
{
	uint64_t f;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
	return f;
}

static void zero(void *p, unsigned n)
{
	volatile uint8_t *b = p;
	for (unsigned i = 0; i < n; i++)
		b[i] = 0;
}

static uint32_t be32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t le64(const uint8_t *p)
{
	uint64_t v = 0;
	for (int i = 7; i >= 0; i--)
		v = v << 8 | p[i];
	return v;
}

static int mem_eq(const void *a, const void *b, unsigned n)
{
	const uint8_t *x = a, *y = b;
	while (n--)
		if (*x++ != *y++)
			return 0;
	return 1;
}

static int str_eq(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

static uint32_t scsi(uint8_t lun, const uint8_t *cdb, unsigned cdb_len, void *buf,
		     uint32_t len, uint32_t *ocs_out, uint32_t *status_out)
{
	static uint8_t tag;
	zero(&utrl[0], sizeof(utrl[0]));
	zero(&ucd, sizeof(ucd));

	ucd.cmd[0] = 0x01;			/* COMMAND UPIU */
	ucd.cmd[1] = len ? 0x40 : 0x00;		/* R (data in) */
	ucd.cmd[2] = lun;
	ucd.cmd[3] = ++tag;
	ucd.cmd[12] = len >> 24;
	ucd.cmd[13] = len >> 16;
	ucd.cmd[14] = len >> 8;
	ucd.cmd[15] = len;
	for (unsigned i = 0; i < cdb_len; i++)
		ucd.cmd[16 + i] = cdb[i];

	uintptr_t ucd_pa = (uintptr_t)&ucd;
	utrl[0].dw0 = (1u << 28) | ((len ? 2u : 0u) << 25) | (1u << 24);
	utrl[0].dw2 = 0x0f;
	utrl[0].ucdba = (uint32_t)ucd_pa;
	utrl[0].ucdbau = (uint32_t)(ucd_pa >> 32);
	utrl[0].resp = (512u / 4) << 16 | (512u / 4);
	if (len) {
		uintptr_t buf_pa = (uintptr_t)buf;
		ucd.prdt[0].addr_lo = (uint32_t)buf_pa;
		ucd.prdt[0].addr_hi = (uint32_t)(buf_pa >> 32);
		ucd.prdt[0].dbc = len - 1;
		utrl[0].prdt = (1024u / 4) << 16 | 1;
	}
	__asm__ volatile("dsb sy" ::: "memory");

	ufs_wr(REG_UTRLDBR, 1);
	uint64_t start = ticks();
	while (ufs_rd(REG_UTRLDBR) & 1) {
		if (ticks() - start > freq()) {
			ufs_wr(REG_UTRLCLR, ~1u);
			return 1;
		}
	}
	__asm__ volatile("dsb sy" ::: "memory");
	*ocs_out = utrl[0].dw2 & 0xff;
	*status_out = ucd.resp[7];
	if (*ocs_out)
		return 2;
	if (ucd.resp[0] != 0x21)
		return 3;
	return *status_out ? 4 : 0;
}

static uint32_t ufs_tur(uint8_t lun, uint32_t *ocs, uint32_t *st)
{
	static const uint8_t cdb[6] = { 0x00 };
	return scsi(lun, cdb, sizeof(cdb), 0, 0, ocs, st);
}

static uint32_t ufs_capacity(uint8_t lun, uint32_t *last, uint32_t *blk,
			     uint32_t *ocs, uint32_t *st)
{
	static const uint8_t cdb[10] = { 0x25 };
	uint32_t rc = scsi(lun, cdb, sizeof(cdb), capbuf, sizeof(capbuf), ocs, st);
	if (!rc) {
		*last = be32(capbuf);
		*blk = be32(capbuf + 4);
	}
	return rc;
}

static uint32_t ufs_read10(uint8_t lun, uint32_t lba, uint16_t count, void *buf,
			   uint32_t len, uint32_t *ocs, uint32_t *st)
{
	uint8_t cdb[10] = {
		0x28, 0, lba >> 24, lba >> 16, lba >> 8, lba, 0, count >> 8, count, 0
	};
	return scsi(lun, cdb, sizeof(cdb), buf, len, ocs, st);
}

static void name16(const uint8_t *e, char *out)
{
	for (int c = 0; c < 16; c++) {
		uint8_t ch = e[2 * c];
		out[c] = (ch >= 32 && ch < 127) ? ch : 0;
		if (!ch)
			break;
	}
	out[16] = 0;
}

static int wanted(const char *name)
{
	static const char *names[] = {
		"boot_a", "boot_b", "super", "userdata", "metadata", "persist",
		"expdb", "vbmeta_a", "vbmeta_b", "userdata2"
	};
	for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		if (str_eq(name, names[i]))
			return 1;
	return 0;
}

static void print_gpt(void)
{
	const uint8_t *gpt = 0;
	if (mem_eq(data + 512, "EFI PART", 8))
		gpt = data + 512;
	else if (mem_eq(data + 4096, "EFI PART", 8))
		gpt = data + 4096;
	if (!gpt) {
		con_str("GPT", "not found in block 0");
		return;
	}

	uint64_t ent_lba = le64(gpt + 72);
	uint32_t nent = le32(gpt + 80);
	uint32_t esz = le32(gpt + 84);
	con_hex2("GPT", "ent_lba=", (uint32_t)ent_lba, "nent=", nent);
	con_hex2("GPT", "esz=", esz, "hdr_lba=", (uint32_t)le64(gpt + 24));

	uint64_t off = ent_lba * 512;
	if (esz != 128 || nent > 128 || off + (uint64_t)nent * esz > DATA_SIZE) {
		con_str("GPT", "entries outside 32K window");
		return;
	}
	const uint8_t *ents = data + off;
	for (uint32_t i = 0; i < nent; i++) {
		const uint8_t *e = ents + i * esz;
		uint64_t first = le64(e + 32);
		uint64_t last = le64(e + 40);
		if (!first)
			continue;
		char name[17];
		name16(e + 56, name);
		if (!wanted(name))
			continue;
		uint32_t mib = (uint32_t)((last - first + 1) / 2048);
		con_hex2(name, "lba=", (uint32_t)first, "MiB=", mib);
	}
}

void test_ufs(void)
{
	con_str("UFS", "read-only probe");
	uint32_t cap = ufs_rd(REG_CAP), ver = ufs_rd(REG_VER);
	uint32_t hcs = ufs_rd(REG_HCS), hce = ufs_rd(REG_HCE);
	con_hex2("UFS", "ver=", ver, "cap=", cap);
	con_hex2("UFS", "hce=", hce, "hcs=", hcs);
	con_hex2("UFS", "is=", ufs_rd(REG_IS), "rsr=", ufs_rd(REG_UTRLRSR));
	con_hex2("UFS", "utrlba=", ufs_rd(REG_UTRLBA), "hi=", ufs_rd(REG_UTRLBAU));
	con_hex2("UFS", "dbr=", ufs_rd(REG_UTRLDBR), "nutrs=", (cap & 0x1f) + 1);

	if (!(hce & 1) || !(hcs & HCS_DP) || !(hcs & HCS_UTRLRDY)) {
		con_str("UFS", "host not enabled/ready: stop");
		return;
	}
	if (ufs_rd(REG_UTRLDBR)) {
		con_str("UFS", "requests outstanding: stop");
		return;
	}

	ufs_wr(REG_UTRLRSR, 0);
	uintptr_t list = (uintptr_t)utrl;
	ufs_wr(REG_UTRLBA, (uint32_t)list);
	ufs_wr(REG_UTRLBAU, (uint32_t)(list >> 32));
	ufs_wr(REG_UTRLRSR, 1);
	con_hex2("UFS", "ours=", (uint32_t)list, "rsr=", ufs_rd(REG_UTRLRSR));
	if (ufs_rd(REG_UTRLBA) != (uint32_t)list ||
	    ufs_rd(REG_UTRLBAU) != (uint32_t)(list >> 32) ||
	    !(ufs_rd(REG_UTRLRSR) & 1)) {
		con_str("UFS", "list base not taken: stop");
		return;
	}

	for (uint32_t lun = 0; lun < 3; lun++) {
		char label[] = "LUN0";
		label[3] = '0' + lun;
		uint32_t ocs = 0, st = 0, rc = 0;
		for (int i = 0; i < 3; i++) {
			rc = ufs_tur((uint8_t)lun, &ocs, &st);
			if (!rc)
				break;
		}
		con_hex2(label, "tur=", rc, "ocs/st=", (ocs << 8) | st);

		uint32_t last = 0, blk = 0;
		rc = ufs_capacity((uint8_t)lun, &last, &blk, &ocs, &st);
		con_hex2(label, "cap=", rc, "last=", last);
		con_hex2(label, "blk=", blk, "ocs/st=", (ocs << 8) | st);
		if (rc || !blk)
			continue;

		uint32_t count = DATA_SIZE / blk;
		if (!count)
			count = 1;
		rc = ufs_read10((uint8_t)lun, 0, (uint16_t)count, data, DATA_SIZE, &ocs, &st);
		con_hex2(label, "read=", rc, "ocs/st=", (ocs << 8) | st);
		if (rc)
			continue;
		con_hex2(label, "mbr=", data[510] | data[511] << 8, "first=", be32(data));
		print_gpt();
	}
}
