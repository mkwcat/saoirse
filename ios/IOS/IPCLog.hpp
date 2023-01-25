// IPCLog.hpp - IOS to PowerPC logging through IPC
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once
#include <System/OS.hpp>
#include <System/Types.h>

class IPCLog
{
public:
    static IPCLog* sInstance;

    static constexpr int printSize = 256;

    IPCLog();
    void Run();
    void Print(const char* buffer);

    /**
     * Notify that a resource is ready. The channel will count the number of
     * resources to make sure everything is started.
     */
    void Notify();

    /**
     * Notify the channel that a storage device was inserted.
     */
    void NotifyDeviceInsertion(u8 id);

    /**
     * Notify the channel that a storage device was removed.
     */
    void NotifyDeviceRemoval(u8 id);

    /**
     * Wait for the request from the channel to start.
     */
    void WaitForStartRequest();

protected:
    void HandleRequest(IOS::Request* req);

    Queue<IOS::Request*> m_ipcQueue;
    Queue<IOS::Request*> m_responseQueue;
    Queue<int> m_startRequestQueue;
};
