#pragma once

#ifdef TARGET_IOS
#   include <types.h>
#else
#   include <gctypes.h>
#endif

#ifdef __cplusplus
    extern "C" {
#endif

typedef struct
{
    u16 dev; /* Device ID (SD, USB, etc. - determined on runtime) */
    u16 fs; /* Filesystem type (like FAT) */
    /* 
     * Filesystem specific info - will move into a union if other filesystems
     * are ever added
     */
    struct {
        /* Data for FatFS */
        u8 flag;
        u8 err;
        u8 obj_attr;
        u8 obj_stat;
        u64 obj_sclust;
        u64 obj_size;
        u64 fptr;
        u64 sect;
        u32 clust;
    } fat;
} EfsFile;

#ifdef __cplusplus
    }
#endif