	.section .rodata.saoirse_ios_elf, "a"
	.balign 4
	.global saoirse_ios_elf
saoirse_ios_elf:
	.incbin "../bin/saoirse_ios.elf"

	.global saoirse_ios_elf_end
saoirse_ios_elf_end:

	.global saoirse_ios_elf_size
	.balign 4
saoirse_ios_elf_size: .long saoirse_ios_elf_end - saoirse_ios_elf
