// TaskThread.hpp
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once
#include <System/OS.hpp>
#include <System/Types.h>

class TaskThread
{
public:
    static s32 __threadProc([[maybe_unused]] void* arg)
    {
        TaskThread* that = reinterpret_cast<TaskThread*>(arg);
        that->taskEntry();
        that->taskSuccess();
        return 0;
    }

    void start(Queue<int>* onDestroyQueue = nullptr)
    {
        if (m_running)
            return;

        m_running = true;
        m_cancelTask = false;
        m_onDestroyQueue = onDestroyQueue;
        m_thread.create(&__threadProc, reinterpret_cast<void*>(this), nullptr,
                        0x1000, 80);
    }

    void stop()
    {
        m_cancelTask = true;
    }

    bool isRunning() const
    {
        return m_running;
    }

    void taskBreak()
    {
        if (m_cancelTask) {
            _destroyThread(0);
        }
    }

    void taskAbort()
    {
        _destroyThread(-1);
    }

    void taskSuccess()
    {
        _destroyThread(0);
    }

    virtual void taskEntry() = 0;

private:
    void _destroyThread(int result)
    {
        m_running = false;
        m_thread.~Thread();
        if (m_onDestroyQueue != nullptr) {
            m_onDestroyQueue->send(result);
        }
    }

    Thread m_thread;
    bool m_cancelTask = false;
    bool m_running = false;
    Queue<int>* m_onDestroyQueue = nullptr;
};