// Arch.cpp - Archive data reader
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#include "Arch.hpp"
#include <cstdlib>
#include <stdio.h>
#include <string.h>

Arch* Arch::s_instance;

bool Arch::getShortName(const char* name, char* out)
{
    if (m_lfnFile == nullptr)
        return false;

    const char* start = m_lfnFile;
    const char* end = m_lfnFile + m_lfnFileSize;

    while (start < end) {
        if (*start == '\n' || *start == ' ') {
            start++;
            continue;
        }

        const char* fnend = strchr(start, '/');
        u32 len = fnend - start;
        if (strlen(name) == len && !strncmp(start, name, len)) {
            snprintf(out, 8, "/%d", start - m_lfnFile);
            return true;
        }
        start = fnend + 1;
    }

    return false;
}

const void* Arch::getFile(const char* name, u32* size)
{
    char shortName[8];
    const char* rname = name;

    if (strlen(name) > 15) {
        if (!getShortName(name, shortName))
            return nullptr;
        rname = shortName;
    }

    for (auto it : m_subfiles) {
        if (strlen(rname) == it.nameLen &&
            !strncmp(rname, it.name, it.nameLen)) {
            if (size != nullptr) {
                *size = it.size;
            }
            return it.data;
        }
    }
    return nullptr;
}

const void* Arch::getFileStatic(const char* name, u32* size)
{
    return Arch::s_instance->getFile(name, size);
}

Arch::Arch(const char* file, u32 size)
{
    m_file = file;
    m_lfnFile = nullptr;

    if (strncmp(m_file, "!<arch>\n", sizeof("!<arch>\n") - 1)) {
        m_valid = false;
        return;
    }

    std::vector<const char*> m_longFileNames;
    const char* start = m_file + sizeof("!<arch>\n") - 1;
    const char* end = start + size;

    while (start + 0x3C <= end) {
        if (*start == '\n') {
            start++;
            continue;
        }

        const char* fnend;
        if (start[0] != '/') {
            fnend = strchr(start + 1, '/');
        } else {
            fnend = strchr(start + 1, ' ');
        }

        Subfile subfile;
        subfile.name = start;
        subfile.nameLen = fnend - start;
        subfile.size = atoi(start + 0x30);
        subfile.data = start + 0x3C;
        m_subfiles.push_back(subfile);

        if (subfile.nameLen == 2 && !strncmp(subfile.name, "//", 2)) {
            m_lfnFile = subfile.data;
            m_lfnFileSize = subfile.size;
        }

        start += 0x3C + subfile.size;
    }

    m_valid = true;
    return;
}
