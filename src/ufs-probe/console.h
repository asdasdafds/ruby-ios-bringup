#pragma once
#include <stdint.h>

/*
 * Minimal console interface used by the UFS probe.
 *
 * The implementation is part of the bring-up shim and is not included in this
 * repository. These declarations are enough to compile the probe against a
 * compatible shim; the framebuffer address is the ruby/LK scan buffer.
 */
#define FB_BASE   0x7cdfc000UL
#define FB_W      1080
#define FB_H      2400
#define FB_STRIDE 4352

#define BLACK  0xff101018U
#define RED    0xffff0000U
#define BLUE   0xff0000a0U
#define WHITE  0xffffffffU
#define YELLOW 0xffffff00U
#define GREEN  0xff00ff00U

extern volatile uint32_t *con_fb;

void con_clear(uint32_t bg);
void con_skip(void);
void con_str(const char *label, const char *s);
void con_hex(const char *label, uint64_t v);
void con_hex2(const char *label, const char *a, uint32_t va,
              const char *b, uint32_t vb);
void con_cpu(const char *label, uint32_t ret, uint32_t aff, uint32_t ent,
             uint32_t midr);
