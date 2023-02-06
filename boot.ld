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
	
	. = 0x81200000;
	
	.text : {
		*(.text*)
		*(.rodata*)
		. = ALIGN(32);
	} : text = 0

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

	vtr = 0xcc002000;
    dcr = 0xcc002002;
    vto = 0xcc00200c;
    vte = 0xcc002010;
    tfbl = 0xcc00201c;
    bfbl = 0xcc002024;
    hsw = 0xcc002048;
    hsr = 0xcc00204a;
    visel = 0xcc00206e;

    ppcmsg = 0xcd000000;
    ppcctrl = 0xcd000004;
    armmsg = 0xcd000008;

    irqmask = 0xcd000034;

    aicr = 0xcd006c00;
}
