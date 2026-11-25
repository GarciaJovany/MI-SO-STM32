ENTRY(Reset_Handler)
MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K
    RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
    .text :
    {
        *(.text*) *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH
    .ARM.exidx : { *(.ARM.exidx*) } > FLASH
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT> FLASH
    _sidata = LOADADDR(.data);
    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*) *(COMMON)
        . = ALIGN(8);
        _ebss = .;
    } > RAM
    _end = _ebss;
    ASSERT(_estack - _end >= 2048, "RAM insuficiente: reservar >=2 KB para pila MSP")
}
