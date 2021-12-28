OUTPUT_FORMAT("elf32-powerpc", "elf32-powerpc", "elf32-powerpc");
OUTPUT_ARCH(powerpc:common);
EXTERN(_start);
ENTRY(_start);

PHDRS
{
	stub PT_LOAD FLAGS(5);
	text PT_LOAD FLAGS(6);
	bss PT_LOAD;
}

SECTIONS
{
	. = 0x80003400;

	.stub : {
		*(.stub)
		. = ALIGN(32);
	} : stub = 0
	
	. = 0x80004000;
	
	.text : {
		*(.text*)
		*(.rodata*)
		. = ALIGN(32);
	} : text = 0

	.bss : {
		PROVIDE(__bss_start = .);
		*(.dynsbss)
		*(.sbss)
		*(.sbss.*)
		*(.gnu.linkonce.sb.*)
		*(.scommon)
		*(.sbss2)
		*(.sbss2.*)
		*(.gnu.linkonce.sb2.*)
		*(.bss)
		*(.bss.*)
		*(.dynbss)
		*(.gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(32);
		PROVIDE (__bss_end = .);
	} : bss = 0
}