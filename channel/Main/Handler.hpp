// Handler.hpp - Various event handlers
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once

#include <gctypes.h>
#include <vector>

class Handler
{
    virtual void stub()
    {
    }
};

class DeviceHandler : public Handler
{
public:
    virtual void OnDeviceInsertion([[maybe_unused]] u8 id)
    {
    }

    virtual void OnDeviceRemoval([[maybe_unused]] u8 id)
    {
    }
};

class HandlerMgr
{
public:
    static std::vector<Handler*> s_handlers;

    static void AddHandler(Handler* handler)
    {
        s_handlers.push_back(handler);
    }

    static void CallDeviceInsertion(u8 id)
    {
        for (auto h : s_handlers) {
            auto h2 = dynamic_cast<DeviceHandler*>(h);
            if (h2 == nullptr)
                continue;
            h2->OnDeviceInsertion(id);
        }
    }

    static void CallDeviceRemoval(u8 id)
    {
        for (auto h : s_handlers) {
            auto h2 = dynamic_cast<DeviceHandler*>(h);
            if (h2 == nullptr)
                continue;
            h2->OnDeviceRemoval(id);
        }
    }
};
