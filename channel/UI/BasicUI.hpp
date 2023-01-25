// BasicUI.hpp - Simple text-based UI
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once

#include <System/Util.h>
LIBOGC_SUCKS_BEGIN
#include <ogc/video.h>
LIBOGC_SUCKS_END

class BasicUI
{
public:
    static BasicUI* s_instance;

    BasicUI();
    void InitVideo();
    void Loop();

    enum class OptionType {
        StartGame,
        Exit,
    };

private:
    enum class OptionStatus {
        Disabled,
        Hidden,
        Enabled,
        Waiting,
        Selected,
    };

    void ClearScreen();
    void DrawTitle();
    OptionStatus GetOptionStatus(OptionType opt);
    void DrawOptions();
    void UpdateOptions();
    void OnSelect(OptionType opt);

private:
    GXRModeObj* m_rmode;
    void* m_xfbConsole;
    void* m_xfbUI;

    int m_selectedOption;
    bool m_cursorEnabled;
    bool m_optionSelected;
};
