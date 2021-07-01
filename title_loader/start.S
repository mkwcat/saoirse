		.text
		.arm
		.global _start
_start:
		ldr     sp, =_stack + 0x400
		push    { lr }
		ldr     r1, =_main
		blx     r1
		ldr     r1, =_exit
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
