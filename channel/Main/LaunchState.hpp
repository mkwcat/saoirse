// LaunchState.hpp - Game launch progress
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once

template <class T>
struct LaunchValue {
    LaunchValue()
    {
        available = false;
    }

    bool available;
    T state;
};

class LaunchState
{
public:
    static LaunchState* Get()
    {
        static LaunchState* instance = nullptr;

        if (instance == nullptr) {
            instance = new LaunchState();
        }

        return instance;
    }

    LaunchValue<bool> DiscInserted;
    LaunchValue<bool> ReadDiscID;
    LaunchValue<bool> SDCardInserted;

    // Unavailable if currently trying to launch. False if failed.
    LaunchValue<bool> LaunchReady;
};
