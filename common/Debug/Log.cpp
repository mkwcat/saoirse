// Log.cpp - Debug log
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include "Log.hpp"
#include <System/OS.hpp>
#include <System/Types.h>
#ifdef TARGET_IOS
#include <IOS/IPCLog.hpp>
#endif
#include <array>
#include <cstring>
#include <stdarg.h>
#include <stdio.h>

#ifdef TARGET_IOS
bool Log::ipcLogEnabled = false;
bool Log::fileLogEnabled = false;
FIL Log::logFile;
#endif

static constexpr std::array<const char*, 3> logColors = {
    "\x1b[37;1m",
    "\x1b[33;1m",
    "\x1b[31;1m",
};
static Mutex* logMutex;
constexpr u32 logMask = 0xFFFFFFFF;
constexpr u32 logLevel = 0;

bool Log::IsEnabled()
{
#ifdef TARGET_IOS
    return ipcLogEnabled || fileLogEnabled;
#else
    return true;
#endif
}

void Log::VPrint(LogSource src, const char* srcStr, LogLevel level,
                 const char* format, va_list args)
{
#ifdef TARGET_IOS
    if (!ipcLogEnabled && !fileLogEnabled)
        return;
#endif
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
        snprintf(&printBuffer[0], printBuffer.size(), "%s[%s] %s\x1b[37;1m",
                 logColors[slvl], srcStr, logBuffer.data());

        if (ipcLogEnabled) {
            IPCLog::sInstance->Print(&printBuffer[0]);
        }

        if (fileLogEnabled) {
            UINT bw = 0;
            // Subtract 7 twice to remove the color codes on both sides
            f_write(&logFile, &logBuffer[0], strlen(&logBuffer[0]), &bw);
            static const char newline = '\n';
            f_write(&logFile, &newline, 1, &bw);
            f_sync(&logFile);
        }
#else
        printf("%s[%s] %s\n\x1b[37;1m", logColors[slvl], srcStr,
               logBuffer.data());
#endif
        logMutex->unlock();
    }
}

void Log::Print(LogSource src, const char* srcStr, LogLevel level,
                const char* format, ...)
{
    va_list args;
    va_start(args, format);
    VPrint(src, srcStr, level, format, args);
    va_end(args);
}