#pragma once

#include <types.h>
#include <os.h>
#include "irse.h"

namespace IOSBoot
{

s32 Entry(u32 entrypoint);
s32 Launch(const void* data, u32 len);

class Log
{
public:
    Log();
    ~Log() {
        reset = true;
        logRM.close();
        // meh
        while (reset) { }
    }

protected:
    static s32 Callback(s32, void*);
    void restartEvent() {
        logRM.ioctlAsync(0, NULL, 0, this->logBuffer, sizeof(this->logBuffer),
            &Callback, reinterpret_cast<void*>(this));
    }

    bool reset = false;
    IOS::ResourceCtrl<s32> logRM{"/dev/stdout"};
    char logBuffer[256];
};

#if 0
void SetupPrintHook();
void ReadPrintHook();
void testIPCRightsPatch();
#endif

}