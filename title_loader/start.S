		.text
		.arm
		.global _start
_start:
		ldr     sp, =_stack + 0x400
		push    { lr }
		ldr     r1, =saoMain
		blx     r1
		ldr     r1, =saoExit
		blx     r1
		pop     { r1 }
		bx      r1

		.pool
		.size   _start, . - _start

		.bss
		.global _stack
_stack:
		.space  0x400
		.size   _stack, . - _stack
