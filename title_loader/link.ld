OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
	ram(rwx)	: ORIGIN = 0x10C00000, LENGTH = 0x20000
}

SECTIONS
{
	.text : {
		. = ALIGN(4);
    	*(.text*)
		*(.syscall*)
	} > ram
	.data : {
		. = ALIGN(4);
		*(.data*)
		. = ALIGN(4);
	} > ram
	.rodata : {
		. = ALIGN(4);
		*(.rodata*)
		. = ALIGN(4);
	} > ram
	.bss : {
		. = ALIGN(4);
		__bss_start = .;
		*(.bss*)
		*(.sbss*)
		. = ALIGN(4);
		__bss_end = .;
	} > ram
	/DISCARD/ :
	{
		*(.ARM*)
		*(.comment)
	}
}