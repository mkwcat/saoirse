// Input.cpp - User input manager
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include "Input.hpp"
#include <System/Util.h>
LIBOGC_SUCKS_BEGIN
#include <ogc/pad.h>
#include <wiiuse/wpad.h>
LIBOGC_SUCKS_END

Input* Input::sInstance;

Input::Input()
{
    PAD_Init();
    WPAD_Init();

    m_lastState = 0;
    m_state = 0;
    m_scanned = false;
}

void Input::ScanButton()
{
    m_lastState = m_state;
    m_state = GetButtonRaw();

    if (!m_scanned) {
        // On the first scan, automatically assume that all buttons pressed are
        // being held,
        m_lastState = m_state;
        // ... except for the debug button.
        m_lastState &= ~BTN_DEBUG;
    }

    m_scanned = true;
}

u32 Input::GetButtonDown()
{
    // Clear buttons that were pressed in the last state.
    return m_state & ~m_lastState;
}

u32 Input::GetButtonUp()
{
    // Clear buttons that weren't pressed in the last state.
    return ~m_state & m_lastState;
}

u32 Input::GetButtonHeld()
{
    return GetButtonRaw();
}

u32 Input::GetButtonRaw()
{
    PAD_ScanPads();
    WPAD_ScanPads();
    u32 down = PAD_ButtonsHeld(0);
    u32 wiimotedown = WPAD_ButtonsHeld(0);

    u32 result = 0;

    if (down & PAD_BUTTON_UP || wiimotedown & WPAD_BUTTON_UP)
        result |= BTN_UP;

    if (down & PAD_BUTTON_DOWN || wiimotedown & WPAD_BUTTON_DOWN)
        result |= BTN_DOWN;

    if (down & PAD_BUTTON_LEFT || wiimotedown & WPAD_BUTTON_LEFT)
        result |= BTN_LEFT;

    if (down & PAD_BUTTON_RIGHT || wiimotedown & WPAD_BUTTON_RIGHT)
        result |= BTN_RIGHT;

    if (down & PAD_BUTTON_A  || wiimotedown & WPAD_BUTTON_A)
        result |= BTN_SELECT;

    if (down & PAD_BUTTON_B || wiimotedown & WPAD_BUTTON_B)
        result |= BTN_BACK;

    if (down & PAD_BUTTON_MENU || wiimotedown & WPAD_BUTTON_HOME)
        result |= BTN_HOME;

    if (down & PAD_TRIGGER_Z || wiimotedown & WPAD_BUTTON_1)
        result |= BTN_DEBUG;

    return result;
}