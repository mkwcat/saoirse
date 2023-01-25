// Config.hpp - Saoirse config
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once

// Config is currently hardcoded

class Config
{
public:
    static Config* s_instance;

    bool IsISFSPathReplaced(const char* path);
    bool IsFileLogEnabled();
    bool BlockIOSReload();
};
