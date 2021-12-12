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

        .size   iosOpenStrncpyHook, . - iosOpenStrncpyHook
