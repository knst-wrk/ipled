
/* Memory configuration
*/
MEMORY
{
    FLASH(rx):
        ORIGIN = 0x8000000,
        LENGTH = 128K

    RAM(rwx):
        ORIGIN = 0x20000000,
        LENGTH = 20K
}

/* Stack size */
_stack_size = 0;

/* Heap size */
_heap_size = 0;

/* Stack end.
The ARM stack grows downwards and _estack points behind the last usable address
in the RAM memory. This is fine because pushing onto the stack first decrements
the stack pointer and then writes the datum. */
_estack = ORIGIN(RAM) + LENGTH(RAM);
