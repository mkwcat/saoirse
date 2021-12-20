        .thumb
        .global iosOpenStrncpyHook
        .section ".kernel", "ax", %progbits
        .type   iosOpenStrncpyHook, function
        .align  2
iosOpenStrncpyHook:
        // Overwrite first parameter
        str     r0, [sp, #0x14]
        .global iosOpenStrncpy
        ldr     r3, =iosOpenStrncpy
        mov     r12, r3
        mov     r3, r10 // pid
        bx      r12
        .pool
        .size   iosOpenStrncpyHook, . - iosOpenStrncpyHook


        .thumb
        .global patchNewCommonKey
        .section ".text.patchNewCommonKey", "ax", %progbits
        .type   patchNewCommonKey, function
        .align  2
patchNewCommonKey:
        mov     r0, #0xB // common key 2 handle
        adr     r1, korean_key
        mov     r2, #16 // sizeof(korean_key)
        ldr     r3, =0x13A79C59
        bx      r3
        .pool
korean_key:
        .byte   0x63, 0xb8, 0x2b, 0xb4, 0xf4, 0x61, 0x4e, 0x2e
        .byte   0x13, 0xf2, 0xfe, 0xfb, 0xba, 0x4c, 0x9b, 0x7e
        .size   patchNewCommonKey, . - patchNewCommonKey