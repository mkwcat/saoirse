#pragma once

#include <System/Types.h>

struct SelImportEntry {
    const char* symbol;
    u32 stub;
};

struct ChannelInitInfo {
    u32 entry;
    SelImportEntry* importTable;
    SelImportEntry* importTableEnd;
};
