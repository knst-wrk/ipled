
/* Sections related to debugging
*/
SECTIONS
{
    /* Stabs debugging sections.  */
    .stab               0 : { *(.stab) }
    .stabstr            0 : { *(.stabstr) }
    .stab.excl          0 : { *(.stab.excl) }
    .stab.exclstr       0 : { *(.stab.exclstr) }
    .stab.index         0 : { *(.stab.index) }
    .stab.indexstr      0 : { *(.stab.indexstr) }
    .comment            0 : { *(.comment) }

    /* DWARF debug sections.
    Symbols in the DWARF debugging sections are relative to the beginning
    of the section so we begin them at 0. */
    /* DWARF 1 */
    .debug              0 : { *(.debug) }
    .line               0 : { *(.line) }

    /* GNU DWARF 1 extensions */
    .debug_srcinfo      0 : { *(.debug_srcinfo) }
    .debug_sfnames      0 : { *(.debug_sfnames) }

    /* DWARF 1.1 and DWARF 2 */
    .debug_aranges      0 : { *(.debug_aranges) }
    .debug_pubnames     0 : { *(.debug_pubnames) }

    /* DWARF 2 */
    .debug_info         0 : { *(.debug_info) *(.gnu.linkonce.wi.*) }
    .debug_abbrev       0 : { *(.debug_abbrev) }
    .debug_line         0 : { *(.debug_line) }
    .debug_frame        0 : { *(.debug_frame) }
    .debug_str          0 : { *(.debug_str) }
    .debug_loc          0 : { *(.debug_loc) }
    .debug_macinfo      0 : { *(.debug_macinfo) }

    /* DWARF 3 */
    .debug_pubtypes     0 : { *(.debug_pubtypes) }
    .debug_ranges       0 : { *(.debug_ranges) }

    /* DWARF 4 */
    .debug_types        0 : { *(.debug_types) }

    /* DWARF 5 */
    .debug_addr         0 : { *(.debug_addr) }
    .debug_line_str     0 : { *(.debug_line_str) }
    .debug_loclists     0 : { *(.debug_loclists) }
    .debug_macro        0 : { *(.debug_macro) }
    .debug_names        0 : { *(.debug_names) }
    .debug_rnglists     0 : { *(.debug_rnglists) }
    .debug_str_offsets  0 : { *(.debug_str_offsets) }
    .debug_sup          0 : { *(.debug_sup) }
    .debug_cu_index     0 : { *(.debug_cu_index) }
    .debug_tu_index     0 : { *(.debug_tu_index) }
}
