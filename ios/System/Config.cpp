// Config.cpp - Saoirse config
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include "Config.hpp"
#include <cstring>

Config* Config::sInstance;

bool Config::IsISFSPathReplaced(const char* path)
{
    // A list of paths to be replaced will be provided by the channel in the
    // future.
    if (strncmp(path, "/title/00010000/", sizeof("/title/00010000/") - 1) == 0)
        return true;
    if (strncmp(path, "/title/00010004/", sizeof("/title/00010004/") - 1) == 0)
        return true;

    return false;
}

bool Config::IsFileLogEnabled()
{
    return true;
}

bool Config::BlockIOSReload()
{
    return false;
}