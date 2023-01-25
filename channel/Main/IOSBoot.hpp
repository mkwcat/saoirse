// IOSBoot.hpp - IOS startup code
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once
#include <Debug/Log.hpp>
#include <System/OS.hpp>
#include <System/Types.h>
#include <System/Util.h>

namespace IOSBoot
{

void Init();
void WaitForIOS();
s32 Entry(u32 entrypoint);
s32 Launch(const void* data, u32 len);
void SafeFlushRange(const void* data, u32 len);
void LaunchSaoirseIOS();
void DebugLaunchReport();

class IPCLog
{
public:
    static IPCLog* s_instance;

    IPCLog();

    int getEventCount() const
    {
        return m_eventCount;
    }

    void startGameIOS();

    void setEventWaitingQueue(Queue<u32>* queue, int count)
    {
        m_eventCount = 0;
        m_eventQueue = queue;
        m_triggerEventCount = count;
    }

protected:
    static s32 HandlerThreadEntry(void* userdata);
    bool handleEvent(s32 result);
    static s32 threadEntry(void* userdata);

    bool reset = false;
    IOS::ResourceCtrl<Log::IPCLogIoctl> logRM{"/dev/saoirse"};
    char logBuffer[256] ATTRIBUTE_ALIGN(32);

    int m_eventCount = 0;
    Queue<u32>* m_eventQueue;
    int m_triggerEventCount = -1;

    struct HandlerReq {
        Log::IPCLogReply cmd;
        char buffer[256];
    };

    Queue<HandlerReq*> m_handlerQueue;

    Thread m_thread;
    Thread m_handlerThread;
};

void SetupPrintHook();
void ReadPrintHook();

} // namespace IOSBoot
