    .section .text.entry,"ax"
    .global _start
_start:
    mov x19, x0
    ldr x0, =0x42000000
    mov sp, x0

    mrs x0, CurrentEL
    and x0, x0, #0xC
    cmp x0, #0x8
    b.eq el2_path
    cmp x0, #0x4
    b.eq el1_path
    cmp x0, #0xC
    b.eq el3_path
    b after_mmu

el2_path:
    ldr x0, =vectors
    msr vbar_el2, x0
    mrs x0, sctlr_el2
    bic x0, x0, #1
    bic x0, x0, #4
    msr sctlr_el2, x0
    isb
    b after_mmu

el1_path:
    ldr x0, =vectors
    msr vbar_el1, x0
    mrs x0, sctlr_el1
    bic x0, x0, #1
    bic x0, x0, #4
    msr sctlr_el1, x0
    isb
    b after_mmu

el3_path:
    ldr x0, =vectors
    msr vbar_el3, x0
    mrs x0, sctlr_el3
    bic x0, x0, #1
    bic x0, x0, #4
    msr sctlr_el3, x0
    isb

after_mmu:
    dsb sy

    ldr x0, =0x10007000
    ldr w1, =0x22000000
    str w1, [x0]
    dsb sy

    mov x0, x19
    bl shim_main

spin_loop:
    ldr x0, =0x10007008
    mov w1, #1
    str w1, [x0]
    mov x2, #0x200000
pet_delay:
    subs x2, x2, #1
    b.ne pet_delay
    b spin_loop

    .section .text.vectors,"ax"
    .align 11
    .global vectors
vectors:
    .rept 16
    b exc_handler
    .space 0x7c, 0
    .endr

    .section .text.handler,"ax"
    .global exc_handler
exc_handler:
    ldr x4, =fault_stage
    ldr w4, [x4]
    ldr w3, =0xffff00ff
    cmp w4, #1
    b.eq exc_c1
    cmp w4, #21
    b.eq exc_c21
    cmp w4, #22
    b.eq exc_c22
    cmp w4, #23
    b.eq exc_c23
    cmp w4, #24
    b.eq exc_c24
    cmp w4, #25
    b.eq exc_c25
    cmp w4, #3
    b.eq exc_c3
    cmp w4, #4
    b.eq exc_c4
    b exc_fill_start

exc_c1:
    ldr w3, =0xffff8000
    b exc_fill_start
exc_c21:
    ldr w3, =0xff0080ff
    b exc_fill_start
exc_c22:
    ldr w3, =0xff00c0c0
    b exc_fill_start
exc_c23:
    ldr w3, =0xff808080
    b exc_fill_start
exc_c24:
    ldr w3, =0xff804000
    b exc_fill_start
exc_c25:
    ldr w3, =0xffff0080
    b exc_fill_start
exc_c3:
    ldr w3, =0xffffff00
    b exc_fill_start
exc_c4:
    ldr w3, =0xff8000ff
    b exc_fill_start

exc_fill_start:
    ldr x0, =0x7ba10000
    ldr x1, =0x1df0000
    add x2, x0, x1
exc_fill:
    str w3, [x0], #4
    cmp x0, x2
    b.lo exc_fill
    dsb sy
    isb
exc_spin:
    b exc_spin
