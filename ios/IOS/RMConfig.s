// RMConfig.s - IOS module configuration
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#define IOS_NOTE_START() \
	.text; \
	.arm; \
	.balign 4; \
	.section .ios_note; \
elf_note:; \
	.long 0; /* Name size */ \
	.long ios_note_end - ios_note; \
	.long 0; /* Note type */ \
ios_note:

#define IOS_NOTE(pid, entry_point, prio, stack_size, stack_top) \
	.long 0xB; \
	.long pid; \
	.long 9; \
	.long entry_point; \
	.long 0x7D; \
	.long prio; \
	.long 0x7E; \
	.long stack_size; \
	.long 0x7F; \
	.long stack_top

#define IOS_NOTE_END() \
	.size elf_note, . - elf_note; \
ios_note_end: 

#define IOS_STACK(name, sSize) \
	.bss; \
	.balign 32; \
	.global name; \
name:; \
	.space sSize; \
	.size name, sSize

IOS_STACK(EntryStack, 0x400);

IOS_NOTE_START();
IOS_NOTE(
	0,
	Entry,
	127,
	0x400,
	EntryStack + 0x400
);
IOS_NOTE_END();
