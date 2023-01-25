// Log.cpp - Debug log
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#include "Log.hpp"
#include <System/OS.hpp>
#include <System/Types.h>
#ifdef TARGET_IOS
#  include <Disk/DeviceMgr.hpp>
#  include <IOS/IPCLog.hpp>
#endif
#include <array>
#include <cstring>
#include <stdarg.h>
#include <stdio.h>

#ifdef TARGET_IOS
bool Log::ipcLogEnabled = false;
#endif

Mutex* Log::logMutex;

static constexpr std::array<const char*, 3> logColors = {
  "\x1b[37;1m",
  "\x1b[33;1m",
  "\x1b[31;1m",
};
static constexpr std::array<char, 3> logChars = {
  'I',
  'W',
  'E',
};

constexpr u32 logMask = 0xFFFFFFFF;
constexpr u32 logLevel = 0;

bool Log::IsEnabled()
{
#ifdef TARGET_IOS
    return ipcLogEnabled || DeviceMgr::s_instance->IsLogEnabled();
#else
    return true;
#endif
}

void Log::VPrint(LogSource src, const char* srcStr, const char* funcStr,
  LogLevel level, const char* format, va_list args)
{
    if (!IsEnabled())
        return;

    if (logMutex == nullptr) {
        logMutex = new Mutex;
    }

    u32 slvl = static_cast<u32>(level);
    u32 schan = static_cast<u32>(src);
    ASSERT(slvl < logColors.size());

    if (level != LogLevel::ERROR) {
        if (!(logMask & (1 << schan)))
            return;
        if (slvl < logLevel)
            return;
    }
    {
        logMutex->lock();

        static std::array<char, 256> logBuffer;
        u32 len = vsnprintf(&logBuffer[0], logBuffer.size(), format, args);
        if (len >= logBuffer.size()) {
            len = logBuffer.size() - 1;
            logBuffer[len] = 0;
        }

        // Remove newline at the end of log
        if (logBuffer[len - 1] == '\n')
            logBuffer[len - 1] = 0;

#ifdef TARGET_IOS
        static std::array<char, 256> printBuffer;

        if (ipcLogEnabled) {
            len = snprintf(&printBuffer[0], printBuffer.size(),
              "%s[%s %s] %s\x1b[37;1m", logColors[slvl], srcStr, funcStr,
              logBuffer.data());
            IPCLog::s_instance->Print(&printBuffer[0]);
        }

        if (DeviceMgr::s_instance->IsLogEnabled()) {
            len = snprintf(&printBuffer[0], printBuffer.size(), "%c[%s %s] %s",
              logChars[slvl], srcStr, funcStr, logBuffer.data());
            DeviceMgr::s_instance->WriteToLog(&printBuffer[0], len);
        }

#else
        printf("%s[%s %s] %s\n\x1b[37;1m", logColors[slvl], srcStr, funcStr,
          logBuffer.data());
#endif
        logMutex->unlock();
    }
}

void Log::Print(LogSource src, const char* srcStr, const char* funcStr,
  LogLevel level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    VPrint(src, srcStr, funcStr, level, format, args);
    va_end(args);
}
