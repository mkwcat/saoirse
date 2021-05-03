		.text
		.arm
		.global IOS_DisableInterrupts
		.fnstart
IOS_DisableInterrupts:
		mrs     r1, cpsr
		and     r0, r1, #0xC0
		orr     r1, r1, #0xC0
		msr     cpsr_c, r1
		bx      lr
		.fnend
		.size   IOS_DisableInterrupts, . - IOS_DisableInterrupts

		.text
		.arm
		.global IOS_RestoreInterrupts
		.fnstart
IOS_RestoreInterrupts:
		mrs     r1, cpsr
		bic     r1, r1, #0xC0
		orr     r1, r1, r0
		msr     cpsr_c, r1
		bx      lr
		.fnend
		.size   IOS_RestoreInterrupts, . - IOS_RestoreInterrupts

		.text
		.arm
		.global IOS_DisableMMU
		.fnstart
IOS_DisableMMU:
		ldr     pc, =0xFFFF2564
		.fnend
		.size   IOS_DisableMMU, . - IOS_DisableMMU

		.text
		.arm
		.global IOS_EnableMMU
		.fnstart
IOS_EnableMMU:
		ldr     pc, =0xFFFF253C
		.fnend
		.pool
		.size   IOS_EnableMMU, . - IOS_EnableMMU
