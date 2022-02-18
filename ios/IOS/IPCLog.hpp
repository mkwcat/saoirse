// IPCLog.hpp - IOS to PowerPC logging through IPC
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
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
    void Notify();

    void WaitForStartRequest();

protected:
    void HandleRequest(IOS::Request* req);

    Queue<IOS::Request*> m_ipcQueue;
    Queue<IOS::Request*> m_responseQueue;
    Queue<int> m_startRequestQueue;
};