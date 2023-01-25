// Arch.hpp - Archive data reader
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once
#include <System/Types.h>
#include <vector>

class Arch
{
public:
    static Arch* sInstance;

    Arch(const char* file, u32 size);
    bool getShortName(const char* name, char* out);
    const void* getFile(const char* name, u32* size = nullptr);
    static const void* getFileStatic(const char* name, u32* size = nullptr);

private:
    const char* m_file;
    bool m_valid;

    struct Subfile {
        const char* name;
        u32 nameLen;
        u32 size;
        const char* data;
    };

    std::vector<Subfile> m_subfiles;
    const char* m_lfnFile;
    u32 m_lfnFileSize;
};
