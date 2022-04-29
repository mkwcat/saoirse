// Log.hpp - Debug log
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once
#include <stdarg.h>
#ifdef TARGET_IOS
#include <FAT/ff.h>
#endif

namespace Log
{

enum class LogSource
{
    Core,
    DVD,
    Loader,
    Payload,
    FST,
    PatchList,
    DiskIO,
    IOS,
    IOS_Loader,
    IOS_DevMgr,
    IOS_USB,
    IOS_EmuFS,
    IOS_EmuDI
};

enum class LogLevel
{
    INFO,
    WARN,
    ERROR
};

enum class IPCLogIoctl
{
    RegisterPrintHook,
    StartGameEvent
};

enum class IPCLogReply
{
    Print,
    Notice,
    Close
};

#ifdef TARGET_IOS
extern bool ipcLogEnabled;
extern bool fileLogEnabled;
extern FIL logFile;
#endif

bool IsEnabled();

void VPrint(LogSource src, const char* srcStr, const char* funcStr,
            LogLevel level, const char* format, va_list args);
void Print(LogSource src, const char* srcStr, const char* funcStr,
           LogLevel level, const char* format, ...);

#define STR(f) #f

#define PRINT(CHANNEL, LEVEL, ...)                                             \
    Log::Print(Log::LogSource::CHANNEL, #CHANNEL, __FUNCTION__,                \
               Log::LogLevel::LEVEL, __VA_ARGS__)

} // namespace Log