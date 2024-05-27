
/* Entry point is Reset_Handler as per CMSIS */
ENTRY(Reset_Handler)

/* Memory configuration */
INCLUDE mem.ld

/* Libraries */
INCLUDE libs.ld

/* Sections */
INCLUDE sections.ld

/* Debug symbols */
INCLUDE debug.ld

/* Discard rest */
INCLUDE discard.ld
