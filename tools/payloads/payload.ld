/* Raw payload loaded by Recovery at 0x41000000; code, then read-only data. */
ENTRY(payload_main)
SECTIONS {
	. = PAYLOAD_BASE;
	.text : { KEEP(*(.text.entry)) *(.text.start) *(.text*) *(.rodata*) . = ALIGN(4); }
	/* The host appends a random 32-bit nonce right after the binary; the payload republishes it. */
	payload_end = .;
	/DISCARD/ : { *(.ARM.*) *(.comment) *(.note*) *(.data*) *(.bss*) }
}
