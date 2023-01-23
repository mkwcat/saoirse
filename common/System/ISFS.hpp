// ISFS.hpp - ISFS types
//   Written by Star
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once

// Definitions
#define NAND_DIRECTORY_SEPARATOR_CHAR '/'

#define NAND_MAX_FILEPATH_LENGTH 64 // Including the NULL terminator
#define NAND_MAX_FILE_DESCRIPTOR_AMOUNT 15

#define NAND_SEEK_SET 0
#define NAND_SEEK_CUR 1
#define NAND_SEEK_END 2

#define EFS_MAX_PATH_LEN 2048

constexpr s32 ISFSMaxPath = NAND_MAX_FILEPATH_LENGTH;

enum class ISFSIoctl {
    Format = 0x1,
    GetStats = 0x2,
    CreateDir = 0x3,
    ReadDir = 0x4,
    SetAttr = 0x5,
    GetAttr = 0x6,
    Delete = 0x7,
    Rename = 0x8,
    CreateFile = 0x9,
    GetFileStats = 0xB,
    GetUsage = 0xC,
    Shutdown = 0xD,

    Direct_Open = 0x1000,
    Direct_DirOpen = 0x1001,
    Direct_DirNext = 0x1002,
};

struct ISFSRenameBlock {
    char pathOld[ISFSMaxPath];
    char pathNew[ISFSMaxPath];
};

struct ISFSAttrBlock {
    u32 ownerId; // UID, title specific
    u16 groupId; // GID, the "maker", for example 01 in RMCE01.
    char path[ISFSMaxPath];
    // Access flags (like IOS::Mode)
    // If the caller's identifiers match UID or GID, use those
    // permissions. Otherwise use otherPerm.
    u8 ownerPerm; // Permissions for UID
    u8 groupPerm; // Permissions for GID
    u8 otherPerm; // Permissions for any other process
    u8 attributes;
    u8 pad[2];
};

struct ISFSDirect_Stat {
    enum {
        RDO = 0x01, // Read only
        HID = 0x02, // Hidden
        SYS = 0x04, // System
        DIR = 0x10, // Directory
        ARC = 0x20, // Archive
    };

    u64 dirOffset;
    u64 size;
    u8 attribute;
    char name[EFS_MAX_PATH_LEN];
};
