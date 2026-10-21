ENTRY(_start)
SECTIONS
{
    . = 0x40081000;
    .text.entry : { KEEP(*(.text.entry)) }
    . = ALIGN(0x800);
    .text.vectors : { KEEP(*(.text.vectors)) }
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.attributes*)
        *(.comment*)
        *(.note*)
    }
}
