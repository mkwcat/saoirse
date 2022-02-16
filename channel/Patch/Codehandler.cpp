#include "Codehandler.hpp"
#include <Main/Arch.hpp>

PatchList::PatchError Codehandler::ImportCodehandler(PatchList* patchList)
{
    u32 size;
    const u8* data = (u8*)Arch::getFileStatic("codehandler.bin", &size);

    return patchList->ImportMemoryPatch(0x80001800, data, data + size);
}

PatchList::PatchError Codehandler::ImportGCT(PatchList* patchList,
                                             const u8* start, const u8* end)
{
    return patchList->ImportMemoryPatch(0x800028B8, start, end);
}