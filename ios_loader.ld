OUTPUT_ARCH(arm)
ENTRY(LoaderEntry)

MEMORY {
	ram(rwx) 	: ORIGIN = 0x10100000, LENGTH = 0x40000
}

SECTIONS
{
	.text : {
		*(.start*)
    	*(.text*)
		*(.syscall*)
		__data_start = .;
		*(.data*)
		*(.rodata*)
		__bss_start = .;
		*(.bss*)
		*(.sbss*)
		__bss_end = .;
		__data_end = .;
	} > ram = 0
	
	/DISCARD/ :
	{
		*(.ARM*)
		*(.comment)
	}
}

PROVIDE(__end__ = .);
PROVIDE(__exidx_start = .);
PROVIDE(__exidx_end = .);