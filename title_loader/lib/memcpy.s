@ Stolen memcpy implementation from IOS
		.text
		.arm
		.global memcpy
		.fnstart
memcpy:
		cmp     r2, #0
		bxeq    lr
		push    {r4-r6}
		orr     r3, r0, r1
		tst     r3, #3
		bne     loc_0x78
		cmp     r2, #15
		bls     loc_0x60
		cmp     r2, #31
		bls     loc_0x48
		push    {r7-r9}
loc_0x2c:
		ldm     r1!, {r3-r9, r12}
		stmia   r0!, {r3-r9, r12}
		sub     r2, r2, #32
		cmp     r2, #31
		bls     loc_0x44
		b       loc_0x2c
loc_0x44:
		pop     {r7, r8, r9}
loc_0x48:
		cmp     r2, #15
		bls     loc_0x60
		ldm     r1!, {r3, r4, r5, r6}
		stmia   r0!, {r3, r4, r5, r6}
		sub     r2, r2, #16
		b       loc_0x48
loc_0x60:
		cmp     r2, #3
		bls     loc_0x78
		ldr     r3, [r1], #4
		str     r3, [r0], #4
		sub     r2, r2, #4
		b       loc_0x60
loc_0x78:
		cmp     r0, #0x1800000
		bcs     loc_0xbc
		mov     r12, #0xFF
loc_0x84:
		cmp     r2, #0
		beq     loc_0xd4
		mov     r6, r0
		and     r3, r6, #3
		ldr     r4, [r6, -r3]!
		lsl     r3, r3, #3
		rsb     r3, r3, #24
		bic     r5, r4, r12, lsl r3
		ldrb    r4, [r1], #1
		orr     r4, r5, r4, lsl r3
		str     r4, [r6]
		add     r0, r0, #1
		sub     r2, r2, #1
		b       loc_0x84
loc_0xbc:
		cmp     r2, #0
		beq     loc_0xd4
		ldrb    r3, [r1], #1
		strb    r3, [r0], #1
		sub     r2, r2, #1
		b       loc_0xbc
loc_0xd4:
		pop     {r4-r6}
		bx      lr
		.fnend
		.size   memcpy, . - memcpy
