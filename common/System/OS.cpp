// OS.cpp - libogc-IOS compatible types and functions
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include "OS.hpp"
#include <System/Types.h>

#ifdef TARGET_IOS

s32 IOS::Resource::ipcToCallbackThread(void* arg)
{
    Queue<Request*>* queue = reinterpret_cast<Queue<Request*>*>(arg);

    while (1) {
        Request* req = queue->receive();
        ASSERT(req != nullptr);
        if (req->cb != nullptr) {
            req->cb(req->result, req->userdata);
        }
        delete req;
    }
}

void IOS::Resource::MakeIPCToCallbackThread()
{
    Queue<Request*>* queue = new Queue<Request*>(16);
    s_toCbQueue = queue->id();
    new Thread(&ipcToCallbackThread, reinterpret_cast<void*>(queue), nullptr,
               0x1000, 80);
}

s32 IOS::Resource::s_toCbQueue = -1;

#else

s32 ipcHeap = iosCreateHeap(0x20000);

#endif