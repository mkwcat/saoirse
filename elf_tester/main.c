#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gccore.h>
//#include <wiiuse/wpad.h>
#include <ogc/machine/processor.h>
#include <ogc/isfs.h>

#include <gctypes.h>
#include <ogc/es.h>


/* Set screen to pink if the PPC resets (this shouldn't happen) */
u32 ppc_reset_stub[5] = {
    0x3C600D80, 0x3C80AAB5,
    0x6084B401, 0x90830024,
    0x48000000
};


extern u8 title_loader_elf[];
extern u32 title_loader_elf_size;

extern u32 es_bin[];
extern u32 es_bin_size;


static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

#if 0
/* Debugging thread */
lwp_t ut_h = (lwp_t) NULL;

void* thread_proc(void* arg)
{
    u32* file = (u32*) 0x91000000;

    printf("debug thread entry\n");

    sleep(1);

	printf("FILE: open = %d, pos = %08Xh\n", file[2], file[3]);
    printf("ES: queue = %d, status = %d\n", es_bin[0xA4 / 4], read32(0));

	sleep(3);
    write32(0x0D800194, read32(0x0D800194) & ~1); // restart
    return NULL;
}
#endif

s32 main(s32 argc, char** argv)
{
    /* Initialize controllers. */
    //WPAD_Init();

    /* Initialize video and the console. */
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb, 20, 20,
                 rmode->fbWidth, rmode->xfbHeight,
                 rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(0);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();
    printf("\x1b[2;0H");

    write16(0x0d8b420a, 0);
    printf("MEM_PROT disabled\n");

    /* I cba to explain what's going on here */
    write32(0x939FB738, 0x49004708);
    write32(0x939FB73C, (u32)(&es_bin) - 0x80000000);
    //write32(0, 0x12345678);

    u32* file = (u32*) 0x91000000;
    memset((void*) file, 0, 32);

    memcpy((void*) 0x80003400, (void*) ppc_reset_stub, sizeof(ppc_reset_stub));
    DCFlushRange((void*) 0x80003400, sizeof(ppc_reset_stub));

    file[0] = 0x46494C45;
    file[1] = title_loader_elf_size;
    memcpy((void*) 0x91000020, title_loader_elf, title_loader_elf_size);
    DCFlushRange((void*) 0x91000000, title_loader_elf_size + 32);

#if 0
    printf("make thread\n");
    LWP_CreateThread(&ut_h, thread_proc, 0, 0, 256, 50);
    LWP_SetThreadPriority(LWP_GetSelf(), 50);
#endif

    printf("do\n");
    u8 devicecert[512] ATTRIBUTE_ALIGN(32); // probably big enough
    s32 ret = ES_GetDeviceCert(devicecert);
    printf("ES_GetDeviceCert() = %d\n", ret);

    sleep(5);
    write32(0x0D800024, 0);
    return 0;
}