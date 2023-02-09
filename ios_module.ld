OUTPUT_ARCH(arm)
__entry = 0;
ENTRY(__entry)

PHDRS {
	ios_note_hdr PT_NOTE AT(0) FLAGS(0);
	text PT_LOAD FLAGS(0x00F00000 | 5);
	data PT_LOAD FLAGS(0x00F00000 | 6);
}

MEMORY {
	phys_ram(rwx) 	: ORIGIN = 0x13670000, LENGTH = 0x80000
	virt_ram(rwx) 	: ORIGIN = 0x7FE80000, LENGTH = 0x80000
}

SECTIONS
{
	.ios_note : {
		KEEP(*(.ios_note))
	} : ios_note_hdr = 0

	.text : {
    	*(.text*)
		*(.syscall*)
	} > virt_ram AT > phys_ram : text = 0

	.data : ALIGN(0x1000) {
		__data_start = .;
		*(.data*)
	} > virt_ram AT > phys_ram : data = 0

	.rodata : ALIGN(32) {
		*(.rodata*)
		/* C++ init */
		. = ALIGN(4);
		_init_array_start = .;
		KEEP(*(.init_array))
		_init_array_end = .;
	} > virt_ram AT > phys_ram : data = 0

	.bss : ALIGN(32) {
		__bss_start = .;
		*(.bss*)
		*(.sbss*)
		__bss_end = .;
		__data_end = .;
	} > virt_ram AT > phys_ram : data = 0
	
	/DISCARD/ :
	{
		*(.ARM*)
		*(.comment)
	}
}
PROVIDE(__end__ = .);
PROVIDE(__exidx_start = .);
PROVIDE(__exidx_end = .);
