// Input.hpp - User input manager
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once

#include <System/Types.h>

class Input
{
public:
    static Input* sInstance;

    Input();

    enum Button
    {
        BTN_UP = 1 << 0,
        BTN_DOWN = 1 << 1,
        BTN_LEFT = 1 << 2,
        BTN_RIGHT = 1 << 3,
        BTN_SELECT = 1 << 4,
        BTN_BACK = 1 << 5,
        BTN_HOME = 1 << 6,

        // Z on GameCube controller.
        BTN_DEBUG = 1 << 7,
    };

    // Updates the state.
    void ScanButton();

    // Only set on initial press.
    u32 GetButtonDown();

    // Only set when lifted.
    u32 GetButtonUp();

    // Always set if pressed.
    u32 GetButtonHeld();

private:
    // Gets the raw button data.
    u32 GetButtonRaw();

    // Previous state.
    u32 m_lastState;
    // Current state.
    u32 m_state;

    // If the buttons have been scanned at least once.
    bool m_scanned;
};