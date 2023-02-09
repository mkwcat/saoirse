OUTPUT_FORMAT("elf32-powerpc", "elf32-powerpc", "elf32-powerpc");
OUTPUT_ARCH(powerpc:common);
EXTERN(main);
ENTRY(main);

PHDRS
{
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
	imports PT_LOAD;
	bss PT_LOAD;
}

SECTIONS
{
	. = 0x81600000;

	.text : {
		KEEP (*(.start))
		*(.text*)
		*(.fimport.*)
	} : text = 0

	.data : {
		. = ALIGN(4);
		SelImportTable = .;
		*(.fimport_table*)
		SelImportTableEnd = .;
		. = ALIGN(32);
		*(.rodata*)
		*(.data*)
		. = ALIGN(32);
	} : data = 0

	.bss : {
		PROVIDE(__bss_start = .);
		*(.sbss)
		*(.sbss.*)
		*(.sbss2)
		*(.sbss2.*)
		*(.bss)
		*(.bss.*)
		. = ALIGN(32);
		PROVIDE (__bss_end = .);
	} : bss = 0

	/DISCARD/ : {
		*(.comment)
	}
}
