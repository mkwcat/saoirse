#define	r0 0
#define	r1 1
#define	sp 1
#define	r2 2
#define	toc 2
#define	r3 3
#define	r4 4
#define	r5 5
#define	r6 6
#define	r7 7
#define	r8 8
#define	r9 9
#define	r10 10
#define	r11 11
#define	r12 12
#define	r13 13
#define	r14 14
#define	r15 15
#define	r16 16
#define	r17 17
#define	r18 18
#define	r19 19
#define	r20 20
#define	r21 21
#define	r22 22
#define	r23 23
#define	r24 24
#define	r25 25
#define	r26 26
#define	r27 27
#define	r28 28
#define	r29 29
#define	r30 30
#define	r31 31



#define SRR0 26
#define SRR1 27
#define PVR 287
#define HID5 944
#define BCR 949
#define L2CR 1017
#define HID0 1008
#define HID4 1011
#define IBAT0U 0x210
#define IBAT0L 0x211
#define IBAT1U 0x212
#define IBAT1L 0x213
#define IBAT2U 0x214
#define IBAT2L 0x215
#define IBAT3U 0x216
#define IBAT3L 0x217
#define DBAT0U 0x218
#define DBAT0L 0x219
#define DBAT1U 0x21A
#define DBAT1L 0x21B
#define DBAT2U 0x21C
#define DBAT2L 0x21D
#define DBAT3U 0x21E
#define DBAT3L 0x21F
#define IBAT4U 0x230
#define IBAT4L 0x231
#define IBAT5U 0x232
#define IBAT5L 0x233
#define IBAT6U 0x234
#define IBAT6L 0x235
#define IBAT7U 0x236
#define IBAT7L 0x237
#define DBAT4U 0x238
#define DBAT4L 0x239
#define DBAT5U 0x23A
#define DBAT5L 0x23B
#define DBAT6U 0x23C
#define DBAT6L 0x23D
#define DBAT7U 0x23E
#define DBAT7L 0x23F

	.text
	.section .stub
	.global _start
_start:
	mfspr   r4, PVR
	rlwinm  r4, r4, 16, 16, 31
	cmplwi  r4, 0x7001
	bne-    callRvlStartup

	mfspr   r3, L2CR
	rlwinm  r3, r3, 0, 0, 31
	mtspr   L2CR, r3
	sync

	mfspr   r3, L2CR
	oris    r3, r3, 0x20
	mtspr   L2CR, r3

L1:
	mfspr   r3, L2CR
	rlwinm  r3, r3, 0, 31, 31
	cmpwi   r3, 0
	bne+    L1

	mfspr   r3, L2CR
	rlwinm  r3, r3, 0, 11, 9
	mtspr   L2CR, r3

L2:
	mfspr   r3, L2CR
	rlwinm  r3, r3, 0, 31, 31
	cmpwi   r3, 0
	bne+    L2
	b       L3

	.balign 32
L3:
	mfspr   r3, HID5
	rlwinm  r3, r3, 0, 1, 31
	mtspr   HID5, r3

	nop
	sync
	nop
	nop
	nop

	mfspr   r3, BCR
	oris    r3, r3, 0x1000
	mtspr   BCR, r3
	li      r4, 255

L4:
	subi    r4, r4, 1
	cmpwi   r4, 0
	bne+    L4
	nop

callRvlStartup:
	lis     r3, rvlStartup@h
	andis.  r3, r3, 0x7FFF
	ori     r3, r3, rvlStartup@l
	mtspr   SRR0, r3
	li      r4, 0
	mtspr   SRR1, r4
	rfi

	.balign 0x100
rvlStartup:
	lis     r4, 0x0011
	ori     r4, r4, 0x0C64
	mtspr   HID0, r4

	lis     r4, 0x0000
	ori     r4, r4, 0x2000
	mtmsr   r4

	mfspr   r4, HID4
	rlwinm  r4, r4, 0, 7, 5
	mtspr   HID4, r4

	mfspr   r3, HID0
	ori     r4, r3, 0xC000
	mtspr   HID0, r4
	isync

	li      r4, 0
	mtspr   DBAT0U, r4
	mtspr   DBAT1U, r4
	mtspr   DBAT2U, r4
	mtspr   DBAT3U, r4
	mtspr   DBAT4U, r4
	mtspr   DBAT5U, r4
	mtspr   DBAT6U, r4
	mtspr   DBAT7U, r4
	isync

	lis     r4, 0x8000
	addi    r4, r4, 0x0000
	mtsr    0, r4
	mtsr    1, r4
	mtsr    2, r4
	mtsr    3, r4
	mtsr    4, r4
	mtsr    5, r4
	mtsr    6, r4
	mtsr    7, r4
	mtsr    8, r4
	mtsr    9, r4
	mtsr    10, r4
	mtsr    11, r4
	mtsr    12, r4
	mtsr    13, r4
	mtsr    14, r4
	mtsr    15, r4

	lis     r4, 0x0000
	addi    r4, r4, 0x0002
	lis     r3, 0x8000
	addi    r3, r3, 0x1FFF
	mtspr   DBAT0L, r4
	mtspr   DBAT0U, r3
	isync
	mtspr   IBAT0L, r4
	mtspr   IBAT0U, r3
	isync

	lis     r4, 0x0000
	addi    r4, r4, 0x002A
	lis     r3, 0xC000
	addi    r3, r3, 0x1FFF
	mtspr   DBAT1L, r4
	mtspr   DBAT1U, r3
	isync

	lis     r4, 0x1000
	addi    r4, r4, 0x0002
	lis     r3, 0x9000
	addi    r3, r3, 0x1FFF
	mtspr   DBAT4L, r4
	mtspr   DBAT4U, r3
	isync
	mtspr   IBAT4L, r4
	mtspr   IBAT4U, r3
	isync

	lis     r4, 0x1000
	addi    r4, r4, 0x002A
	lis     r3, 0xD000
	addi    r3, r3, 0x1FFF
	mtspr   DBAT5L, r4
	mtspr   DBAT5U, r3
	isync

#if 0
	lis     r4, 0x0000
	addi    r4, r4, 0x0001
	li      r3, 0x0000
	addi    r3, r3, 0x001F
	mtspr   DBAT3L, r4
	mtspr   DBAT3U, r3
	isync
	mtspr   IBAT3L, r4
	mtspr   IBAT3U, r3
	isync
#endif

	lis     sp, _stack_end@h
	ori     sp, sp, _stack_end@l

	lis     r3, load@ha
	addi    r3, r3, load@l
	mtspr   SRR0, r3

	mfmsr   r4
	ori     r4, r4, 0x30
	mtspr   SRR1, r4
	rfi

.section .bss
.global _stack
.global _stack_end
_stack:
.space 0x1000
_stack_end:
