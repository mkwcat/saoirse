// BasicUI.cpp - Simple text-based UI
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include "BasicUI.hpp"
#include <Main/LaunchState.hpp>
#include <Main/Saoirse.hpp>
#include <UI/Input.hpp>
#include <UI/debugPrint.h>
#include <cstdio>
#include <cstdlib>
LIBOGC_SUCKS_BEGIN
#include <ogc/consol.h>
#include <ogc/machine/processor.h>
#include <ogc/system.h>
#include <wiiuse/wpad.h>
LIBOGC_SUCKS_END

BasicUI* BasicUI::sInstance;

struct OptionDisplay {
    const char* title;
    BasicUI::OptionType type;
};

OptionDisplay options[] = {
    {
        "Start Game",
        BasicUI::OptionType::StartGame,
    },
    {
        "Test EmuFS",
        BasicUI::OptionType::TestFS,
    },
    {
        "Exit",
        BasicUI::OptionType::Exit,
    },
};

static int GetOptionCount()
{
    return (OptionDisplay*)((u32)(&options) + sizeof(options)) - options;
}

BasicUI::BasicUI()
{
    m_rmode = nullptr;
    m_xfbConsole = nullptr;
    m_xfbUI = nullptr;
}

void BasicUI::InitVideo()
{
    // Initialize video.
    VIDEO_Init();

    // Get preferred video mode.
    m_rmode = VIDEO_GetPreferredMode(NULL);

    // Allocate framebuffers.
    m_xfbConsole = MEM_K0_TO_K1(SYS_AllocateFramebuffer(m_rmode));
    m_xfbUI = MEM_K0_TO_K1(SYS_AllocateFramebuffer(m_rmode));

    // Initialize debug console.
    console_init(m_xfbConsole, 20, 20, m_rmode->fbWidth, m_rmode->xfbHeight,
                 m_rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    // Configure render mode.
    VIDEO_Configure(m_rmode);

    // Set framebuffer to UI.
    VIDEO_SetNextFramebuffer(m_xfbUI);

    // Clear UI framebuffer
    ClearScreen();

    // Disable VI black.
    VIDEO_SetBlack(0);

    VIDEO_Flush();
    VIDEO_WaitVSync();

    if (m_rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();

    printf("\x1b[2;0H");

    // Draw text to the screen.
    DebugPrint_Init(m_xfbUI, m_rmode->fbWidth, m_rmode->xfbHeight);
}

void BasicUI::Loop()
{
    m_cursorEnabled = true;
    m_optionSelected = false;
    m_selectedOption = 0;

    while (true) {
        UpdateOptions();
        DrawTitle();
        DrawOptions();

        if (m_optionSelected) {
            OnSelect(options[m_selectedOption].type);
        }

        Input::sInstance->ScanButton();
        u32 buttonDown = Input::sInstance->GetButtonDown();
        u32 buttonUp = Input::sInstance->GetButtonUp();

        if (buttonDown & Input::BTN_DEBUG) {
            // Switch XFB to console.
            VIDEO_SetNextFramebuffer(m_xfbConsole);
            VIDEO_Flush();
        }

        if (buttonUp & Input::BTN_DEBUG) {
            // Switch XFB to UI.
            VIDEO_SetNextFramebuffer(m_xfbUI);
            VIDEO_Flush();
        }

        VIDEO_Flush();
        VIDEO_WaitVSync();
    }
}

void BasicUI::ClearScreen()
{
    for (int i = 0; i < m_rmode->fbWidth * m_rmode->xfbHeight * 2; i += 4) {
        write32(reinterpret_cast<u32>(m_xfbUI) + i, BACKGROUND_COLOUR);
    }
}

void BasicUI::DrawTitle()
{
    DebugPrint_Printf(2, 3, "Saoirse Launcher");

    if (!LaunchState::Get()->DiscInserted.available) {
        DebugPrint_Printf(3, 3, "Game ID: <please wait...>");
        return;
    }

    if (!LaunchState::Get()->DiscInserted.state) {
        DebugPrint_Printf(3, 3, "Game ID: <no disc>       ");
        return;
    }

    if (!LaunchState::Get()->ReadDiscID.available) {
        DebugPrint_Printf(3, 3, "Game ID: <please wait...>");
        return;
    }

    if (!LaunchState::Get()->ReadDiscID.state) {
        DebugPrint_Printf(3, 3, "Game ID: <error>         ");
        return;
    }

    DebugPrint_Printf(3, 3, "Game ID: %.6s           ",
                      (const char*)0x80000000);
}

BasicUI::OptionStatus BasicUI::GetOptionStatus(OptionType opt)
{
    if (m_optionSelected && options[m_selectedOption].type == opt)
        return OptionStatus::Selected;

    switch (opt) {
    case OptionType::StartGame:
    case OptionType::TestFS:
        if (!LaunchState::Get()->DiscInserted.available)
            return OptionStatus::Waiting;

        if (!LaunchState::Get()->DiscInserted.state)
            return OptionStatus::Disabled;

        if (!LaunchState::Get()->LaunchReady.available)
            return OptionStatus::Waiting;

        if (!LaunchState::Get()->LaunchReady.state)
            return OptionStatus::Disabled;

        return OptionStatus::Enabled;

    case OptionType::Exit:
        return OptionStatus::Enabled;

    default:
        return OptionStatus::Hidden;
    }
}

void BasicUI::DrawOptions()
{
    int optionsCount = GetOptionCount();

    int line = 5;

    for (int i = 0; i < optionsCount; i++) {
        OptionStatus status = GetOptionStatus(options[i].type);

        if (status == OptionStatus::Hidden)
            continue;

        const char* statusStr = "";
        switch (status) {
        case OptionStatus::Disabled:
        default:
            statusStr = "<unavailable>   ";
            break;

        case OptionStatus::Enabled:
            statusStr = "                ";
            break;

        case OptionStatus::Waiting:
            statusStr = "<unavailable...>";
            break;

        case OptionStatus::Selected:
            statusStr = "<please wait...>";
            break;
        }

        DebugPrint_Printf(line, 3, "%c %s %s",
                          m_cursorEnabled && m_selectedOption == i ? '>' : ' ',
                          options[i].title, statusStr);

        line++;
    }
}

void BasicUI::UpdateOptions()
{
    if (!m_cursorEnabled)
        return;

    u32 btn = Input::sInstance->GetButtonDown();

    if (btn & Input::BTN_UP) {
        for (int selection = m_selectedOption - 1;
             selection != m_selectedOption; selection--) {
            if (selection < 0)
                selection = GetOptionCount() - 1;

            if (GetOptionStatus(options[selection].type) !=
                OptionStatus::Hidden) {
                m_selectedOption = selection;
                break;
            }
        }
    }

    if (btn & Input::BTN_DOWN) {
        for (int selection = m_selectedOption + 1;
             selection != m_selectedOption; selection++) {
            if (selection >= GetOptionCount())
                selection = 0;

            if (GetOptionStatus(options[selection].type) !=
                OptionStatus::Hidden) {
                m_selectedOption = selection;
                break;
            }
        }
    }

    if (btn & Input::BTN_SELECT) {
        if (GetOptionStatus(options[m_selectedOption].type) ==
            OptionStatus::Enabled) {
            m_cursorEnabled = false;
            m_optionSelected = true;
        }
    }
}

void BasicUI::OnSelect(OptionType opt)
{
    switch (opt) {
    case OptionType::StartGame:
        VIDEO_WaitVSync();
        WPAD_Shutdown();
        LaunchGame();
        break;

    case OptionType::TestFS:
        VIDEO_WaitVSync();
        TestISFSReadDir();
        break;

    case OptionType::Exit:
        VIDEO_WaitVSync();
        exit(0);
        break;
    }

    m_cursorEnabled = true;
    m_optionSelected = false;
}