
/* Standard objects to link in.
*/
GROUP
(
    /* C run time initialization and finalization snippets */
    crti.o
    crtn.o

    /* crt-style constructors and destructors */
    crtbegin.o
    crtend.o

    /* Compiler support */
    libgcc.a
)
