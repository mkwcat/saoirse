#pragma once
#include <System/Types.hpp>

using EntryPoint = void (*)();

struct ApploaderInfo {
    using LogFunction = int (*)(const char* fmt, ...);
    using InitFunction = void (*)(LogFunction log_callback);
    using MainFunction = int (*)(void** dest, int* size, int* offset);
    using FinalFunction = EntryPoint (*)();
    using EntryFunction = void (*)(InitFunction*, MainFunction*,
                                   FinalFunction*);

    u32 _00;
    u32 _04;
    u32 _08;
    u32 _0C;

    EntryFunction payload_entrypoint;
    u32 payload_size;
    u32 _18;
    u32 _1C;
};