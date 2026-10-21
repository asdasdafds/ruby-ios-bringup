    .section .text
    .global _start
_start:
    mrs x0, CurrentEL
    and x0, x0, #0xC
    cmp x0, #0x8
    b.eq el2
    cmp x0, #0x4
    b.eq el1
    b after_mmu

el2:
    mrs x0, sctlr_el2
    bic x0, x0, #1
    bic x0, x0, #4
    msr sctlr_el2, x0
    isb
    b after_mmu

el1:
    mrs x0, sctlr_el1
    bic x0, x0, #1
    bic x0, x0, #4
    msr sctlr_el1, x0
    isb

after_mmu:
    dsb sy

    ldr x0, =0x10007000
    ldr w1, =0x22000000
    str w1, [x0]
    dsb sy

    ldr x0, =0x7ba10000
    ldr x1, =0x1df0000
    add x2, x0, x1
    ldr w3, =0xffff0000

1:
    str w3, [x0], #4
    cmp x0, x2
    b.lo 1b

    dsb sy

    ldr x0, =0x7ba10000
    add x2, x0, x1

2:
    dc cvac, x0
    add x0, x0, #64
    cmp x0, x2
    b.lo 2b

    dsb sy
    isb

3:
    ldr x0, =0x10007008
    mov w1, #1
    str w1, [x0]
    mov x2, #0x200000

4:
    subs x2, x2, #1
    b.ne 4b
    b 3b
