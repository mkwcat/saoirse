// Boot.cpp - Saoirse Loader
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#include "Archive.hpp"
#include "ES.hpp"
#include "ICache.hpp"
#include "IOS.hpp"
#include "LzmaDec.h"
#include "Sel.hpp"
#include <Boot/AddressMap.hpp>
#include <Boot/DCache.hpp>
#include <Boot/Init.hpp>
#include <Debug/Console.hpp>
#include <Debug/Debug_VI.hpp>
#include <cstring>
#include <iterator>
#include <stdio.h>
#include <stdlib.h>

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

static inline void ClearWords(u32* data, u32 count)
{
    while (count--) {
        asm volatile("dcbz    0, %0\n"
                     //"sync\n"
                     "dcbf    0, %0\n" ::"r"(data));
        data += 8;
    }
}

static inline void CopyWords(u32* dest, u32* src, u32 count)
{
    u32 value = 0;
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

IOS::ES::TMD s_shopTmd alignas(32);
u64 s_shopId = 0x0001000248414241;

const char s_contentPath[] = "/title/00000000/00000000/content/00000000.app";

struct ContentPath {
    char str[sizeof(s_contentPath)];
};

template <class T>
void ToHexString(T v, char* out)
{
    for (u32 i = 1; i < sizeof(T) * 2 + 1; i++) {
        u8 j = 0xF & (v >> (sizeof(T) * 8 - i * 4));
        if (j < 0xA)
            out[i - 1] = j + '0';
        else
            out[i - 1] = j - 0xA + 'a';
    }
}

void GetContentPath(u64 titleID, u32 cid, ContentPath* out)
{
    memcpy(out->str, s_contentPath, sizeof(s_contentPath));

    ToHexString<u32>(titleID >> 32, out->str + sizeof("/title/") - 1);
    ToHexString<u32>(titleID, out->str + sizeof("/title/00000000/") - 1);
    ToHexString<u32>(
      cid, out->str + sizeof("/title/00000000/00000000/content/") - 1);
}

bool ReadWiiShopTMD()
{
    // TODO: Korean Wii Shop Channel

    IOS::File file_tmd(
      "/title/00010002/48414241/content/title.tmd", IOS::Mode::Read);
    if (!file_tmd.ok()) {
        Console::Print("\nERROR: Failed to open Wii Shop TMD\n");
        return false;
    }

    IOS::File::Stats stats;
    if (file_tmd.getStats(&stats) < 0) {
        Console::Print("\nERROR : Wii Shop TMD get stats failed\n");
        return false;
    }

    if (stats.size > sizeof(IOS::ES::TMD)) {
        Console::Print("\nERROR : Wii Shop TMD is too large.\n");
        return false;
    }

    if (file_tmd.read(&s_shopTmd, stats.size) != s32(stats.size)) {
        Console::Print("\nERROR : Failed to read Wii Shop TMD.\n");
        return false;
    }

    if (s_shopTmd.numContents < 1) {
        Console::Print("\nERROR : Invalid Wii Shop TMD contents.\n");
        return false;
    }

    return true;
}

DOL s_shopDol alignas(32);

bool LoadWiiShopDOL()
{
    // Read index 1 as that should be the DOL as loaded by the NAND loader.
    if (s_shopTmd.numContents < 2) {
        Console::Print("\nERROR : Wii Shop does not have a DOL.\n");
        return false;
    }

    ContentPath path alignas(32) = {};
    GetContentPath(s_shopId, s_shopTmd.contents[1].id, &path);

    IOS::File file_dol(path.str, IOS::Mode::Read);
    if (!file_dol.ok()) {
        Console::Print("\nERROR: Failed to open Wii Shop DOL.\n");
        return false;
    }

    if (file_dol.read(&s_shopDol, sizeof(s_shopDol)) != sizeof(s_shopDol)) {
        Console::Print("\nERROR : Failed to read Wii Shop DOL header.\n");
        return false;
    }

    ClearWords((u32*) s_shopDol.dol_bss_addr, s_shopDol.dol_bss_size / 4);

    for (int i = 0; i < 7 + 11; i++) {
        if (s_shopDol.dol_sect_size[i] == 0)
            continue;

        if (file_dol.seek(s_shopDol.dol_sect[i], SEEK_SET) !=
            s32(s_shopDol.dol_sect[i])) {
            Console::Print("\nERROR : Failed to seek Wii Shop DOL.\n");
            return false;
        }

        if (file_dol.read((void*) s_shopDol.dol_sect_addr[i],
              s_shopDol.dol_sect_size[i]) != s32(s_shopDol.dol_sect_size[i])) {
            Console::Print("\nERROR : Failed to read Wii Shop DOL.\n");
            return false;
        }

        ICache::Invalidate(
          (void*) s_shopDol.dol_sect_addr[i], s_shopDol.dol_sect_size[i]);
    }

    return true;
}

u8 s_shopArcHeader[0x1000];
u32 s_selSize;

bool LoadWiiShopSEL()
{
    if (s_shopTmd.numContents < 3) {
        Console::Print("\nERROR : Wii Shop does not have a main archive.\n");
        return false;
    }

    ContentPath path alignas(32) = {};
    GetContentPath(s_shopId, s_shopTmd.contents[2].id, &path);

    IOS::File file_arc(path.str, IOS::Mode::Read);
    if (!file_arc.ok()) {
        Console::Print("\nERROR: Failed to open Wii Shop ARC.\n");
        return false;
    }

    IOS::File::Stats stats_arc;
    if (file_arc.getStats(&stats_arc) < 0) {
        Console::Print("\nERROR : Wii Shop ARC get stats failed.\n");
        return false;
    }

    if (file_arc.read(s_shopArcHeader, 32) != 32) {
        Console::Print("\nERROR : Failed to read Wii Shop ARC header.\n");
        return false;
    }

    s32 arcDataStart = *(u32*) (s_shopArcHeader + 0xC);
    if (arcDataStart > 0x1000) {
        Console::Print("\nERROR : Wii Shop ARC FST size too large.\n");
        return false;
    }

    if (file_arc.read(s_shopArcHeader + 32, arcDataStart - 32) !=
        arcDataStart - 32) {
        Console::Print("\nERROR : Failed to read Wii Shop ARC FST.\n");
        return false;
    }

    Archive archive(s_shopArcHeader, stats_arc.size);

    auto entry = archive.get("arc/FINAL/main.sel");
    Archive::File* file = std::get_if<Archive::File>(&entry);
    if (!file) {
        Console::Print("\nERROR : Failed to find the SEL file.\n");
        return false;
    }

    if (file_arc.seek(file->offset, SEEK_SET) != s32(file->offset)) {
        Console::Print("\nERROR : Failed to seek Wii Shop ARC.\n");
        return false;
    }

    if (file_arc.read((void*) SEL_ADDRESS, file->size) != s32(file->size)) {
        Console::Print("\nERROR : Failed to read Wii Shop ARC subfile.\n");
        return false;
    }

    s_selSize = file->size;

    return true;
}

extern u8 LoaderArchive[];
extern u32 LoaderArchiveSize;

u32 s_bootArcSize;

bool DecompressArchive()
{
    ELzmaStatus status;

    u32 compressedSize = LoaderArchiveSize;
    size_t destLen = BOOT_ARC_MAXLEN;
    size_t inLen = compressedSize - 5;
    SRes ret = LzmaDecode((u8*) BOOT_ARC_ADDRESS, &destLen, LoaderArchive + 0xD,
      &inLen, LoaderArchive, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status, 0);

    if (ret != SZ_OK) {
        return false;
    }

    s_bootArcSize = destLen;

    return true;
}

u32 GetSymbol(Sel& sel, const char* name)
{
    auto symbol = sel.GetSymbol(name);
    if (symbol == std::nullopt) {
        Console::Print("\nERROR : Failed to find symbol '");
        Console::Print(name);
        Console::Print("'\n");
        return 0;
    }

    // Get the real address from the section offset
    u32 address = 0;

    switch (symbol->section) {
    case 1:
        // _f_init
        address = s_shopDol.dol_sect_addr[0] + symbol->offset;
        break;

    case 2:
        // _f_text
        address = s_shopDol.dol_sect_addr[1] + symbol->offset;
        break;

    default:
        Console::Print("\nERROR : Relocate to symbol from illegal section '");
        Console::Print(name);
        Console::Print("'\n");
        return 0;
    }

#if 0
    Console::Print("\n'");
    Console::Print(name);
    Console::Print("' at 0x");
    char hexstr[9];
    hexstr[8] = 0;
    ToHexString(address, hexstr);
    Console::Print(hexstr);
    Console::Print(" = ");
    ToHexString(*(u32*) (address | 0xC0000000), hexstr);
    Console::Print(hexstr);
#endif

    return address;
}

void Launch()
{
    Debug_VI::InitFirst();

    Console::Init();
    Console::Print("\n\nSaoirse Loader!\n\n\n");
    IOS::Init();

    Console::Print("Load boot archive... ");
    if (!DecompressArchive()) {
        Console::Print("\nERROR : Decompression failed.\n");
        return;
    }
    Archive archive((u8*) BOOT_ARC_ADDRESS, s_bootArcSize);
    Console::Print("done\n");

    Console::Print("Start IOS loader... ");
    auto entry = archive.get("./ios_loader.bin");
    Archive::File* file = std::get_if<Archive::File>(&entry);
    if (!file) {
        Console::Print("\nERROR : Failed to get the IOS boot payload.\n");
        return;
    }

    memcpy((void*) IOS_BOOT_ADDRESS, (u8*) BOOT_ARC_ADDRESS + file->offset,
      file->size);
    IOS::SafeFlush((void*) IOS_BOOT_ADDRESS, file->size);

    entry = archive.get("./ios_module.elf");
    file = std::get_if<Archive::File>(&entry);
    if (!file) {
        Console::Print("\nERROR : Failed to get the IOS module.\n");
        return;
    }

    *(u32*) IOS_FILE_INFO_ADDRESS = BOOT_ARC_ADDRESS + file->offset;
    *(u32*) (IOS_FILE_INFO_ADDRESS + 4) = file->size;
    IOS::SafeFlush((void*) IOS_FILE_INFO_ADDRESS, IOS_FILE_INFO_MAXLEN);
    IOS::SafeFlush((void*) BOOT_ARC_ADDRESS + file->offset, file->size);

    if (!IOS::BootstrapEntry()) {
        Console::Print("\nERROR : Failed to launch the IOS boot payload.\n");
        return;
    }
    Console::Print("done\n");

    Console::Print("Read Wii Shop Channel TMD... ");
    if (!ReadWiiShopTMD()) {
        return;
    }
    Console::Print("done\n");

    Console::Print("Load Wii Shop Channel DOL... ");
    if (!LoadWiiShopDOL()) {
        return;
    }
    Console::Print("done\n");

    Console::Print("Load Wii Shop Channel SEL... ");
    if (!LoadWiiShopSEL()) {
        return;
    }
    Console::Print("done\n");

    Console::Print("Load Saoirse channel... ");
    entry = archive.get("./ppc_channel.bin");
    file = std::get_if<Archive::File>(&entry);
    if (!file) {
        Console::Print("\nERROR : Failed to get the channel payload.\n");
        return;
    }

    memcpy(
      (void*) 0x81600000, (u8*) BOOT_ARC_ADDRESS + file->offset, file->size);
    DCache::Flush((void*) 0x81600000, file->size);
    ICache::Invalidate((void*) 0x81600000, file->size);

    ChannelInitInfo* info = ((ChannelInitInfo * (*) (void) ) 0x81600000)();

    Sel sel((const u8*) SEL_ADDRESS, file->size);

    s32 importCount = std::distance(info->importTable, info->importTableEnd);

    for (s32 i = 0; i < importCount; i++) {
        u32 address = GetSymbol(sel, info->importTable[i].symbol);
        if (address == 0) {
            return;
        }

        // Patch the stub function to a branch
        *(u32*) (info->importTable[i].stub | 0xC0000000) =
          0x48000000 |
          ((address - u32(info->importTable[i].stub)) & 0x03FFFFFC);
    }

    // Patch main to go to the new entry point
    u32 address = GetSymbol(sel, "main");
    if (address == 0) {
        return;
    }

    *(u32*) (address | 0xC0000000) =
      0x48000000 | ((info->entry - address) & 0x03FFFFFC);

    // Patch DVDCheckDevice
    address = GetSymbol(sel, "__DVDCheckDevice");
    if (address != 0) {
        *(u32*) (address | 0xC0000000) = 0x38600001; // li r3, 1
        *(u32*) ((address + 4) | 0xC0000000) = 0x4E800020; // blr
    }

    Console::Print("done\n");

    IOS::Shutdown();

    u32* mem1 = (u32*) 0x80000000;
    memset(mem1, 0, 0x100);
    mem1[0x20 / 4] = 0x0D15EA5E; // "disease"
    mem1[0x24 / 4] = 0x00000001;
    mem1[0x28 / 4] = 0x01800000;
    mem1[0x2C / 4] = 1 + ((*(u32*) 0xCC00302C) >> 28);
    mem1[0x34 / 4] = 0x817FEC60;
    mem1[0xF0 / 4] = 0x01800000;
    mem1[0xF8 / 4] = 0x0E7BE2C0;
    mem1[0xFC / 4] = 0x2B73A840;
    DCache::Flush(mem1, 0x100);

    memset(mem1 + 0x3000 / 4, 0, 0x400);
    mem1[0x30D8 / 4] = 0xFFFFFFFF;
    mem1[0x30E4 / 4] = 0x00008201;
    mem1[0x3100 / 4] = 0x01800000;
    mem1[0x3104 / 4] = 0x01800000;
    mem1[0x3108 / 4] = 0x81800000;
    mem1[0x3110 / 4] = 0x81600000;
    mem1[0x3114 / 4] = 0xDEADBEEF;
    mem1[0x3118 / 4] = 0x04000000;
    mem1[0x311C / 4] = 0x04000000;
    mem1[0x3120 / 4] = 0x93600000;
    mem1[0x3124 / 4] = 0x933E0000;
    mem1[0x3128 / 4] = 0x93400000;
    mem1[0x312C / 4] = 0xDEADBEEF;
    mem1[0x3130 / 4] = 0x935E0000;
    mem1[0x3134 / 4] = 0x93600000;
    mem1[0x3138 / 4] = 0x00000011;
    mem1[0x313C / 4] = 0xDEADBEEF;
    mem1[0x3140 / 4] = 0xFFFF | ((s_shopTmd.iosID & 0xFFFF) << 16);
    mem1[0x3144 / 4] = 0x00030310;
    mem1[0x3148 / 4] = 0x93600000;
    mem1[0x314C / 4] = 0x93620000;
    mem1[0x3150 / 4] = 0xDEADBEEF;
    mem1[0x3154 / 4] = 0xDEADBEEF;
    mem1[0x3158 / 4] = 0x0000FF01;
    mem1[0x315C / 4] = 0x80AD0113;
    mem1[0x3188 / 4] = 0xFFFF | ((s_shopTmd.iosID & 0xFFFF) << 16);
    DCache::Flush(mem1 + 0x3000 / 4, 0x400);

    ((void (*)(void)) s_shopDol.dol_entry_point)();
}

extern "C" void load()
{
    // TODO: Hardware init

    Launch();
    while (true) {
    }
}
