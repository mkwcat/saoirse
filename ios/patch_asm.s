        .thumb
        .global iosOpenStrncpyHook
        .section ".kernel", "ax", %progbits
        .type   iosOpenStrncpyHook, function
        .align  2
iosOpenStrncpyHook:
        // Overwrite first parameter
        str     r0, [sp, #0x14]

        push    {lr}

        .global iosOpenStrncpy
        ldr     r3, =iosOpenStrncpy
        mov     r12, r3

        mov     r3, r10
        push    {r3} // pid
        add     r3, sp, #0

        blx     r12

        pop     {r3}
        mov     r10, r3

        pop     {r1}
        bx      r1
        .pool
        .size   iosOpenStrncpyHook, . - iosOpenStrncpyHook