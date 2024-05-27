
/* Output sections configuration
*/
SECTIONS
{
    /* Text */
    .text :
    {
        . = ALIGN(4);
        _stext = .;

        /* Interrupt vector table */
        KEEP( *(.isr_vector) )
        ASSERT(. > _stext, "Interrupt vector is empty");

        /* crt initialization and finalization */
        KEEP( crti.o(.init) )
        KEEP( EXCLUDE_FILE(crti.o crtn.o) *(SORT(.init.*) .init) )
        KEEP( crtn.o(.init) )

        KEEP( crti.o(.fini) )
        KEEP( EXCLUDE_FILE(crti.o crtn.o) *(SORT(.fini.*) .fini) )
        KEEP( crtn.o(.fini) )

        /* crt-style constructors and destructors */
        KEEP( crtbegin.o(.ctors) )
        KEEP( EXCLUDE_FILE(crtbegin.o crtend.o) *(.ctors SORT(.ctors.*)) )
        KEEP( crtend.o(.ctors) )

        KEEP( crtbegin.o(.dtors) )
        KEEP( EXCLUDE_FILE(crtbegin.o crtend.o) *(.dtors SORT(.dtors.*)) )
        KEEP( crtend.o(.dtors) )

        /* Application text */
        *(.text)
        *(.text.*)

        *(.eh_frame*)

        . = ALIGN(4);
        _etext = .;
    } > FLASH


    /* Read-only data */
    .rodata : ALIGN(4)
    {
        /* Data sections to be copied to RAM */
        . = ALIGN(4);
        __data_regions_array_start = .;
        LONG( LOADADDR(.data) );
        LONG( ADDR(.data) );
        LONG( ADDR(.data) + SIZEOF(.data) );
        __data_regions_array_end = .;

        /* BSS sections to be zeroed */
        . = ALIGN(4);
        __bss_regions_array_start = .;
        LONG( ADDR(.bss) );
        LONG( ADDR(.bss) + SIZEOF(.bss) );
        __bss_regions_array_end = .;

        /* ARM-style initialization and finalization function pointer tables */
        . = ALIGN(4);
        __preinit_array_start = .;
        KEEP( *(SORT(.preinit_array_sysinit.*)) )
        KEEP( *(.preinit_array_sysinit) )
        KEEP( *(SORT(.preinit_array_platform.*)) )
        KEEP( *(.preinit_array_platform) )
        KEEP( *(SORT(.preinit_array.*)) )
        KEEP( *(.preinit_array) )
        __preinit_array_end = .;

        . = ALIGN(4);
        __init_array_start = .;
        KEEP( *(SORT(.init_array.*)) )
        KEEP( *(.init_array) )
        __init_array_end = .;

        . = ALIGN(4);
        __fini_array_start = .;
        KEEP( *(.fini_array) )
        KEEP( *(SORT(.fini_array.*)) )
        __fini_array_end = .;

        /* Other read-only data */
        *(.rodata)
        *(.rodata.*)

        *(.data.__global_locale)
    } > FLASH


    /* Relocated data */
    . = ALIGN(4);
    _sidata = .;
    .data :
    {
        . = ALIGN(4);
        _sdata = .;

        *(.data)
        *(.data.*)

        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH


    /* Zero-initialized data */
    .bss(NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;

        *(.bss)
        *(.bss.*)
        *(COMMON)

        . = ALIGN(4);
        _ebss = .;
    } > RAM


    /* Uninitialized data */
    .noinit(NOLOAD) :
    {
        *(.noinit)
        *(.noinit.*)
    } > RAM


    /* Program break */
    . = ALIGN(4);
    end = .;
}
