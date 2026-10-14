/* Raw payload loaded by Recovery at 0x41000000; code, then read-only data. */
ENTRY(payload_main)
SECTIONS {
	. = 0x41000000;
	.text : { *(.text.start) *(.text*) *(.rodata*) }
	/DISCARD/ : { *(.ARM.*) *(.comment) *(.note*) *(.data*) *(.bss*) }
}
