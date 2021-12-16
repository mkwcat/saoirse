#pragma once

#include "irse.h"
#include <os.h>
#include <types.h>

namespace IOSBoot
{

s32 Entry(u32 entrypoint);
s32 Launch(const void* data, u32 len);

class Log
{
public:
    Log();

    int getEventCount() const
    {
        return m_eventCount;
    }
    void restartEvent()
    {
        logRM.ioctlAsync(0, NULL, 0, this->logBuffer, sizeof(this->logBuffer),
                         &Callback, reinterpret_cast<void*>(this));
    }
    void setEventWaitingQueue(Queue<u32>* queue, int count) {
        m_eventQueue = queue;
        m_triggerEventCount = count;
    }

protected:
    static s32 Callback(s32, void*);

    bool reset = false;
    IOS::ResourceCtrl<s32> logRM{"/dev/stdout"};
    char logBuffer[256];

    int m_eventCount = 0;
    Queue<u32>* m_eventQueue;
    int m_triggerEventCount = -1;
};

#if 0
void SetupPrintHook();
void ReadPrintHook();
void testIPCRightsPatch();
#endif

} // namespace IOSBoot