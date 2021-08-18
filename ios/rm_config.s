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

IOS_STACK(Log_RMStack, 0x400);
IOS_STACK(FS_RMStack, 0x400);
IOS_STACK(DI_RMStack, 0x400);

IOS_NOTE_START();
/* Debug Log Process (/dev/stdout) */
IOS_NOTE(
	0,
	Log_StartRM,
	40,
	0x400,
	Log_RMStack + 0x400
);
/* External Filesystem/Storage */
IOS_NOTE(
	0,
    FS_StartRM,
	80,
	0x400,
	FS_RMStack + 0x400
);
/* Drive Interface Proxy */
IOS_NOTE(
	0,
	DI_StartRM,
	80,
	0x400,
	DI_RMStack + 0x400
);
IOS_NOTE_END();