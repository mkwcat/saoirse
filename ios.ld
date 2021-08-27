OUTPUT_ARCH(arm)
__entry = 0;
ENTRY(__entry)

PHDRS {
	ios_note_hdr PT_NOTE AT(0) FLAGS(0);
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(0x00F00000 | 6);
}

MEMORY {
	exe(rx) 	: ORIGIN = 0x13700000, LENGTH = 0x20000
	ram(rw) 	: ORIGIN = 0x13700000 + 0x20000, LENGTH = 0x30000
}

SECTIONS
{
	.ios_note : {
		KEEP(*(.ios_note))
	} : ios_note_hdr = 0

	.text : {
    	*(.text*)
		*(.syscall*)
	} > exe : text = 0

	.data : ALIGN(4) {
		*(.data*)
	} > ram : data = 0
	.rodata : ALIGN(4) {
		*(.rodata*)
		/* C++ init */
		_init_array_start = .;
		KEEP(*(.init_array))
		_init_array_end = .;
	} > ram
	.bss : ALIGN(4) {
		__bss_start = .;
		*(.bss*)
		*(.sbss*)
		__bss_end = .;
	} > ram
	/DISCARD/ :
	{
		*(.ARM*)
		*(.comment)
	}
}
PROVIDE(__end__ = .);
PROVIDE(__exidx_start = .);
PROVIDE(__exidx_end = .);
