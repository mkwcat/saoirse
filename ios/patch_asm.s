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
        mov     r2, #0x14 // sizeof(korean_key)
        ldr     r3, =0x13A79C59
        bx      r3
        .pool

korean_key:
        .byte   0x63
        .byte   0xb8
        .byte   0x2b
        .byte   0xb4
        .byte   0xf4
        .byte   0x61
        .byte   0x4e
        .byte   0x2e
        .byte   0x13
        .byte   0xf2
        .byte   0xfe
        .byte   0xfb
        .byte   0xba
        .byte   0x4c
        .byte   0x9b
        .byte   0x7e
        .size   patchNewCommonKey, . - patchNewCommonKey