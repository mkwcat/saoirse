// load.c - LZMA loader stub
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include "LzmaDec.h"
#include <ogc/cache.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned char u8;
typedef unsigned int u32;

extern const u8 channel_dol_lzma[];
extern u32 channel_dol_lzma_end;

void LoaderAbort()
{
    // Do something
    int* i = (int*)0x90000000;
    int* j = (int*)0x90000000;
    while (1) {
        *i = *j + 1;
    }
}

#define DECODE_ADDR ((u8*)(0x81200000))

typedef struct {
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

static inline void clearWords(u32* data, u32 count)
{
    while (count--) {
        asm volatile("dcbz    0, %0\n"
                     //"sync\n"
                     "dcbf    0, %0\n" ::"r"(data));
        data += 8;
    }
}

static inline void copyWords(u32* dest, u32* src, u32 count)
{
    register u32 value = 0;
    while (count--) {
        asm volatile("dcbz    0, %1\n"
                     //"sync\n"
                     "lwz     %0, 0(%2)\n"
                     "stw     %0, 0(%1)\n"
                     "lwz     %0, 4(%2)\n"
                     "stw     %0, 4(%1)\n"
                     "lwz     %0, 8(%2)\n"
                     "stw     %0, 8(%1)\n"
                     "lwz     %0, 12(%2)\n"
                     "stw     %0, 12(%1)\n"
                     "lwz     %0, 16(%2)\n"
                     "stw     %0, 16(%1)\n"
                     "lwz     %0, 20(%2)\n"
                     "stw     %0, 20(%1)\n"
                     "lwz     %0, 24(%2)\n"
                     "stw     %0, 24(%1)\n"
                     "lwz     %0, 28(%2)\n"
                     "stw     %0, 28(%1)\n"
                     "dcbf    0, %1\n" ::"r"(value),
                     "r"(dest), "r"(src));
        dest += 8;
        src += 8;
    }
}

__attribute__((noreturn)) void load()
{
    // From libogc, this will initialize the L2 cache.
    extern void __InitCache();
    __InitCache();
    ICFlashInvalidate();

    ELzmaStatus status;

    u32 channel_dol_lzma_size =
        (const u8*)&channel_dol_lzma_end - channel_dol_lzma;
    size_t destLen = 0x700000;
    size_t inLen = channel_dol_lzma_size - 5;
    SRes ret = LzmaDecode(DECODE_ADDR, &destLen, channel_dol_lzma + 0xD, &inLen,
                          channel_dol_lzma, LZMA_PROPS_SIZE, LZMA_FINISH_END,
                          &status, 0);

    if (ret != SZ_OK)
        LoaderAbort();

    DOL* dol = (DOL*)DECODE_ADDR;
    clearWords((u32*)dol->dol_bss_addr, dol->dol_bss_size / 4);

    for (int i = 0; i < 7 + 11; i++) {
        if (dol->dol_sect_size[i] != 0) {
            copyWords((u32*)dol->dol_sect_addr[i],
                      (u32*)(DECODE_ADDR + dol->dol_sect[i]),
                      (dol->dol_sect_size[i] / 4) / 8);
        }
    }

    (*(void (*)(void))dol->dol_entry_point)();
    while (1) {
    }
}