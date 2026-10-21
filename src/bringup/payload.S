    .section .text
    .global _start
_start:
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
    wfi
    b 3b
