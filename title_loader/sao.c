#include <types.h>
#include <string.h>
#include <ios.h>
#include <es.h>
#include <isfs.h>
#include <rvl/hollywood.h>
#include <debug/debugPrint.h>

#define ADDR_CONTENT_MAP      ((void*) 0x10001000)
#define ADDR_TMD              ((void*) 0x10041000)
#define ADDR_IOS_BOOT         ((void*) 0x10100000)
#define ADDR_SAO_BOOT         ((void*) 0x10C00000)
#define ADDR_SYSMENU_BOOT     ((void*) 0x11000000)
#define ADDR_KERNEL           ((void*) 0xFFFF0000)

#define STATEFLAGS_PATH "/title/00000001/00000002/data/state.dat"
#define SYSMENU_ID 0x000100014C554C5A // LULZ
//#define SYSMENU_ID 0x0000000100000002
#define YUV_BLUE ((255 << 24) | (107 << 16) | (29 << 8))

#define SYSCALL_ADDRESS (0xFFFF93D0 + 0x70 * 4)
s32 kernelExec(void* func, ...);

u32 block1[6];
u32 block2[6];

typedef struct
{
    union {
        struct {
            u32 dol_text[7];
            u32 dol_data[11];
        };
        u32 dol_sect[7 + 11];
    };
    union {
        struct {
            u32 dol_text_addr[7];
            u32 dol_data_addr[11];
        };
        u32 dol_sect_addr[7 + 11];
    };
    union {
        struct {
            u32 dol_text_size[7];
            u32 dol_data_size[11];
        };
        u32 dol_sect_size[7 + 11];
    };
    u32 dol_bss_addr;
    u32 dol_bss_size;
    u32 dol_entry_point;
    u32 dol_pad[0x1C / 4];
} DOL;

#define DOL_ADDR(dol, sect) \
    ((void*) ((char*) dol + dol->dol_sect[sect]))

struct ancastImage
{
    u32 magic;
    u32 null;
    u32 sig_offset;
    u32 null2;
    u32 null3[0x10 / 4];
    u32 sig_type;
    u8 sig[0x38];
    u32 null4[0x44 / 4];
    u16 null5;
    u8 null6;
    u8 null7;
    u32 unk_a4;
    u32 hash_type;
    u32 size;
    u8 hash[0x14];
    u32 null8[0x3C / 4];
};

#define ANCAST_MAGIC 0xEFA282D9

/* IOS boot content hashes */
static const u32 hashes_Boot[] =
{
    0x2D0239D0, 0x7180CA98, 0xBF51F5F2, 0xDE1AA9D1, 0x3E96E5A8
};
static const u32 hashesCount_Boot = 1;

static bool checknull(u32* buffer, u32 size)
{
    while ((u32) buffer < (u32) buffer + size)
    {
        if (*buffer != 0)
            return false;
        buffer++;
    }
    return true;
}

static void fmthex(char* out, u32 value)
{
    for (s32 i = 0; i < 8; i++)
    {
        char v = (value >> (32 - (i * 4 + 4))) & 0xF;
        *out++ = (v >= 0 && v <= 9) ? v + 0x30 : v + 0x57;
    }
}

s32 irow = 4;
#define Dprintf(...) DebugPrint_Printf(irow += 3, 0, __VA_ARGS__)
#define Rprintf(...) DebugPrint_Printf(irow += 3, 0, __VA_ARGS__)

/* STAR */
#define SAOIRSE_TITLEID 0x0001000153544152

#define SAOIRSE_CID_BANNER 0
#define SAOIRSE_CID_WUBOOT 1
#define SAOIRSE_CID_BOOT   2
#define SAOIRSE_CID_ELF    3

#define U64_LO(id) ((u32) (id))
#define U64_HI(id) ((u32) ((id) >> 32))


/* Loads the IOS boot content into memory. */
static s32 saoLoadIOS(void)
{
    struct contentMapEntry
    {
        char cid[8];
        u8 hash[0x14];
    } * contentMap = (struct contentMapEntry*) ADDR_CONTENT_MAP;
    u8* hash;
    u32 contentMapSize;
    s32 ret = 0, fd = -1, i;
    char path[] = "/shared1/00000000.app";
    Fstats stat;

    ret = ISFS_Open("/shared1/content.map", 1);
    if (ret < 0) {
        Dprintf("saoLoadIOS: ISFS_Open content.map failed\n");
        goto end;
    }
    fd = ret;

    ret = ISFS_GetFileStats(fd, &stat);
    if (ret != IOS_SUCCESS) {
        Dprintf("saoLoadIOS: ISFS_GetFileStats content.map failed\n");
        ret = ret < 0 ? ret : -5;
        goto end;
    }

    contentMapSize = stat.length;
    if (contentMapSize > 0x40000) {
        Dprintf("saoLoadIOS: content.map size too large: %X\n", contentMapSize);
        ret = -5;
        goto end;
    }

    ret = ISFS_Read(fd, (void*) contentMap, contentMapSize);
    if (ret < 0) {
        Dprintf("saoLoadIOS: ISFS_Read content.map failed\n");
        goto end;
    }

    ISFS_Close(fd);
    fd = -1;

    /* [TODO] Choose a different hash if this is a Wii U lol */
    hash = (u8*) hashes_Boot;

    for (i = 0;
         i < (contentMapSize / sizeof(struct contentMapEntry));
         i++)
    {
        if (!memcmp(contentMap[i].hash, hash, 0x14))
        {
            memcpy(path + sizeof("/shared/"), contentMap[i].cid, 8);

            ret = ISFS_Open(path, 1);
            if (ret < 0) {
                Dprintf("saoLoadIOS: ISFS_Open failed\n");
                goto end;
            }
            fd = ret;

            ret = ISFS_GetFileStats(fd, &stat);
            if (ret != IOS_SUCCESS) {
                Dprintf("saoLoadIOS: ISFS_GetFileStats failed\n");
                ret = ret < 0 ? ret : -5;
                goto end;
            }

            if (stat.length > 0x400000) {
                Dprintf("saoLoadIOS: Content too large! (%08X)\n",
                        stat.length);
                ret = -5;
                goto end;
            }
            
            ret = ISFS_Read(fd, ADDR_IOS_BOOT, stat.length);
            if (ret != stat.length) {
                Dprintf("saoLoadIOS: ISFS_Read failed\n");
                ret = ret < 0 ? ret : -5;
                goto end;
            }
            
            /* [TODO] Should SHA1 here and print an error if the hash doesn't
             * match (not a security check) */
            ret = 0;
            goto end;
        }
    }
    Dprintf("saoLoadIOS: Could not find IOS hash!\n");
    ret = -101;

end:
    if (fd >= 0)
        ISFS_Close(fd);

    return ret;
}


/* Get boot content for a title. Sets vwii to true if the title is a vWii title
 * (for ancast images) */
static s32 saoGetBootContent(u64 titleID, bool* vwii)
{
    s32 ret, fd = -1;
    char path[64] = "/title/00000000/00000000/content/";
    Fstats stat;
    TMD* meta = ADDR_TMD;

    *vwii = false;

    /* Path stuff */
    fmthex(path + sizeof("/title/") - 1, U64_HI(titleID));
    fmthex(path + sizeof("/title/00000000/") - 1, U64_LO(titleID));
    memcpy(path + sizeof("/title/00000000/00000000/content/") - 1,
           "title.tmd", sizeof("/title.tmd"));

    ret = ISFS_Open(path, IOS_OPEN_READ);
    if (ret < 0) {
        Dprintf("saoGetBootContent: ISFS_Open failed\n");
        goto end;
    }
    fd = ret;

    ret = ISFS_GetFileStats(fd, &stat);
    if (ret != IOS_SUCCESS) {
        Dprintf("saoGetBootContent: ISFS_GetFileStats failed\n");
        ret = ret < 0 ? ret : -5;
        goto end;
    }

    /* Must be room for at least one content */
    if (stat.length > 0x1000 || stat.length < 0x1E4 + 0x24) {
        Dprintf("saoGetBootContent: Invalid TMD size: %d\n", stat.length);
        ret = -1;
        goto end;
    }

    ret = ISFS_Seek(fd, 0x140, IOS_SEEK_SET);
    if (ret != 0x140) {
        Dprintf("saoGetBootContent: ISFS_Seek failed\n");
        ret = ret < 0 ? ret : -5;
        goto end;
    }

    ret = ISFS_Read(fd, meta, stat.length - 0x140);
    if (ret != stat.length - 0x140) {
        Dprintf("saoGetBootContent: ISFS_Read failed\n");
        ret = ret < 0 ? ret : -5;
        goto end;
    }

    if (stat.length - 0x1E4 != meta->numContents * 0x24
     || meta->bootIndex >= meta->numContents) {
        Dprintf("saoGetBootContent: Invalid TMD\n");
        ret = -1;
        goto end;
    }

    ret = meta->contents[meta->bootIndex].cid;
    *vwii = meta->vwiiTitle;

end:
    if (fd >= 0)
        ISFS_Close(fd);

    return ret;
}


/* Verify the DOL by checking that each section is within the accepted range in
 * MEM1, and no sections overlap */
static bool saoVerifyDol(DOL* dol, u32 size)
{
    s32 i, j;
    /* Use u64 to prevent overflowing/underflowing (although this isn't
     * necessary as it would always fail the DOL size check probably) */
    u64 saddr, eaddr;

    /* This check should've already been done but we'll do it anyway */
    if (size < 0x100) {
        Dprintf("saoVerifyDol: DOL size < 0x100\n");
        return false;
    }

    /* Time to check each section buffer */
    for (i = 0; i < 7 + 11; i++)
    {
        if (dol->dol_sect_size[i])
        {
            if (dol->dol_sect[i] & 0x1f)
                return false;

            saddr = (u64) dol->dol_sect_addr[i];
            eaddr = (u64) dol->dol_sect_addr[i] + (u64) dol->dol_sect_size[i];

            if (saddr & 0x1f || eaddr & 0x1f)
                return false;

            if ((u64) dol->dol_sect[i] + (u64) dol->dol_sect_size[i] >= size)
                return false;
            if (saddr < 0x80003400)
                return false;
            if (eaddr > 0x81800000)
                return false;

            /* Check if the section overlaps with any other section */
            for (j = 0; j < 7 + 11; j++)
            {
                if (j == i || !dol->dol_sect_size[j])
                    continue;
            
                if (saddr <= dol->dol_sect_addr[j]
                   && eaddr > dol->dol_sect_addr[j])
                    return false;
                
                /* Check that neither saddr or eaddr are within the buffer */
                if (saddr >= (u64) dol->dol_sect_addr[j]
                 && saddr  < (u64) dol->dol_sect_addr[j]
                           + (u64) dol->dol_sect_size[j])
                    return false;
                if (eaddr >= (u64) dol->dol_sect_addr[j]
                 && eaddr  < (u64) dol->dol_sect_addr[j]
                           + (u64) dol->dol_sect_size[j])
                    return false;
            }
        }
    }

    /* Check BSS buffer. Note: Don't check for overlapping here because BSS can
     * and actually does overlap with other sections usually */
    if ((u64) dol->dol_bss_addr != 0) {
        if (dol->dol_bss_size == 0)
            return false;
        if ((u64) dol->dol_bss_addr < (u64) 0x80003400)
            return false;
        if ((u64) dol->dol_bss_addr + (u64) dol->dol_bss_size > 0x81800000)
            return false;
    }

    /* If we've reached here then it's all good */
    return true;
}


/* Verifies the integrity of an ancast DOL
 * (This does not check the hash) */
static bool saoVerifyAncast(DOL* dol)
{
    struct ancastImage* ancast;

    if (dol->dol_data_addr[0] != 0x81330000
        || dol->dol_data_size[0] < 0x100
        || *(u32*) DOL_ADDR(dol, 7) != ANCAST_MAGIC
    ) {
        Dprintf("saoVerifyAncast: Not a vWii ancast image.\n");
        return false;
    }

    ancast = (struct ancastImage*) DOL_ADDR(dol, 7);
    if (ancast->null != 0
        || ancast->sig_offset != 0x20
        || ancast->null2 != 0
        || !checknull(ancast->null3, 0x10)
        || ancast->sig_type != 1
        || !checknull(ancast->null4, 0x44)
        || ancast->null5 != 0
        || ancast->null6 != 0
        || ancast->null7 != 0
        || ancast->unk_a4 != 0x13
        || ancast->hash_type != 2
        || ancast->size == 0
        || !checknull(ancast->null8, 0x3C)
    ) {
        Dprintf("saoVerifyAncast: Invalid vWii ancast image.\n");
        return false;
    }

    return true;
}


/* Patch the Kernel for the IOS_KernelExec syscall */
s32 saoPatchKernel(void)
{
    s32 ret, queue;

    queue = IOS_CreateMessageQueue((u32*) SYSCALL_ADDRESS, 1);
    if (queue < 0)
        return queue;
    
    ret = IOS_SendMessage(queue, (u32) &kernelExec, 0);
    IOS_DestroyMessageQueue(queue);
    return ret;
}

void saoMapRegion(void* _0, u32* block)
{
    ((void (*)(u32*)) 0xFFFF5179)(block);
}

void saoMain(u32 arg)
{
    s32 ret, fd = -1;
    u32 size;
    DOL* dol;
    bool isWiiU, vwii, loaded = false, preloader = false;
    char path[64];
    Fstats stat;

    IOS_SetThreadPriority(0, 80);

    if (saoPatchKernel() < 0)
        return;

    //IOS_Write32(HW_VISOLID, ((84 << 24) | (255 << 16) | (76 << 8)) | 1);

    /* Map uncached MEM1 */
    block1[0] = 0x00000000; // physical address
    block1[1] = 0x80000000; // virtual address
    block1[2] = 0x10000000; // length
    block1[3] = 15; // domain (shared)
    block1[4] = 3; // access (read/write)
    block1[5] = 0; // is cached
    IOS_KernelExec((void*) &saoMapRegion, block1);

    /* Map uncached MEM2 */
    block2[0] = 0x10000000; // physical address
    block2[1] = 0x90000000; // virtual address
    block2[2] = 0x04000000; // length
    block2[3] = 15; // domain (shared)
    block2[4] = 3; // access (read/write)
    block2[5] = 0; // is cached
    IOS_KernelExec((void*) &saoMapRegion, block2);

    ES_InitLib();

    //DebugPrint_ClearScreen();

    isWiiU = (*(u32*) 0x0D8005A0 >> 16) == 0xCAFE;

    ret = saoLoadIOS();
    if (ret < 0)
        goto end;

    /* Load from NAND. */
    if (!loaded)
    {
        /* Check for Preloader. */
        fd = ISFS_Open("/title/00000001/00000002/content/title_or.tmd", 0);
        if (fd != ISFS_EEXIST) {
            Dprintf("title_or.tmd exists, assuming Preloader\n");
            preloader = true;
            if (fd >= 0)
                IOS_Close(fd);
        }

        ret = saoGetBootContent(0x100000002, &vwii);
        if (ret < 0) {
            Rprintf("[ERROR] Could not find boot content: %d\n", ret);
            goto end;
        }

        /* Preloader renames the real Wii Menu content to + 0x10000000 */
        if (preloader)
            ret += 0x10000000;

        memcpy(path, "/title/00000001/00000002/content/",
              sizeof("/title/00000001/00000002/content/"));
        fmthex(path + sizeof("/title/00000001/00000002/content/") - 1, ret);
        memcpy(path + sizeof("/title/00000001/00000002/content/") + 7,
               ".app", sizeof(".app"));

        fd = ISFS_Open(path, IOS_OPEN_READ);
        if (fd >= 0) {
            if (ISFS_GetFileStats(fd, &stat) != IOS_SUCCESS
             || (size = stat.length) < 0x100 || size > 0x1000000
             || ISFS_Read(fd, (void*) ADDR_SYSMENU_BOOT, size) != size
            )
                Rprintf("[ERROR] Could not load boot content.\n");
            else {
                loaded = saoVerifyDol(dol, size);
                if (!loaded)
                    Rprintf("[ERROR] Invalid system menu boot content.\n");
            }
        } else
            Rprintf("%s\n", path);

        /* If it's vWii, it will be an ancast image. But let's make sure we're
         * not booting an ancast image on a Wii. */
        if (loaded && vwii) {
            if (!isWiiU) {
                Rprintf("[ERROR] Wii Menu is a vWii title.\n");
                goto end;
            }
            loaded = saoVerifyAncast(dol);
        }
    }

    if (!loaded)
        goto end;
    
    Dprintf("It loaded!\n");


end:
    if (fd >= 0)
        ISFS_Close(fd);
}

void saoExit(void)
{
    u32 msg[8];
    u32 cnt;
    TicketView view;

    Dprintf("");
    //while (1) { }

    if (ES_GetNumTicketViews(SYSMENU_ID, &cnt) < 0 || cnt != 1
     || ES_GetTicketViews(SYSMENU_ID, &view, cnt) < 0
     || ES_LaunchTitle(SYSMENU_ID, &view) < 0
    ) {
        /* Launch system menu failed! */
        IOS_Clear32(HW_RESETS, RSTBINB);
    }
}