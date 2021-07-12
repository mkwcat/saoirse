OUTPUT_ARCH(arm)
__entry = 0;
ENTRY(__entry)

PHDRS {
	ios_note_hdr PT_NOTE AT(0) FLAGS(0);
	text PT_LOAD;
	data PT_LOAD;
}

MEMORY {
	exe(rx) 	: ORIGIN = 0x13AE0000, LENGTH = 0x10000
	ram(rw) 	: ORIGIN = 0x13AE0000 + 0x10000, LENGTH = 0x50000
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