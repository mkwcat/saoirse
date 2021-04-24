		.text
		.arm
		.global _start
_start:
		ldr     sp, =_stack
		bl      saoMain
		b       saoExit

		.pool
		.size   _start, . - _start

		.bss
		.global _stack
_stack:
		.space  0x400
		.size   _stack, . - _stack
