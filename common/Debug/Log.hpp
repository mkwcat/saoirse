// Log.hpp - Debug log
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#pragma once

#include <stdarg.h>
#ifdef TARGET_IOS
#  include <FAT/ff.h>
#endif
#include <System/OS.hpp>

namespace Log
{

enum class LogSource {
    Core,
    DVD,
    Loader,
    Payload,
    FST,
    PatchList,
    Riivo,
    IOS,
    IOS_Loader,
    IOS_DevMgr,
    IOS_SDCard,
    IOS_USB,
    IOS_EmuFS,
    IOS_EmuDI,
    IOS_EmuES,
};

enum class LogLevel {
    INFO,
    WARN,
    ERROR
};

enum class IPCLogIoctl {
    RegisterPrintHook,
    StartGameEvent,
    SetTime,
};

enum class IPCLogReply {
    Close,
    Print,
    Notice,
    DevInsert,
    DevRemove,
};

#ifdef TARGET_IOS
extern bool ipcLogEnabled;
#endif

extern Mutex* logMutex;

bool IsEnabled();

void VPrint(LogSource src, const char* srcStr, const char* funcStr,
  LogLevel level, const char* format, va_list args);
void Print(LogSource src, const char* srcStr, const char* funcStr,
  LogLevel level, const char* format, ...);

#define STR(f) #f

#define PRINT(CHANNEL, LEVEL, ...)                                             \
  Log::Print(Log::LogSource::CHANNEL, #CHANNEL, __FUNCTION__,                  \
    Log::LogLevel::LEVEL, __VA_ARGS__)

} // namespace Log
