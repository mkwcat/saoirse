#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <gccore.h>
#include "os.h"
#include <mutex>

enum class LogS {
    Core, DVD, Loader, Payload
};
enum class LogL { INFO, WARN, ERROR };

namespace irse
{

// TODO: Config logging level for individual sources
void LogConfig(u32 srcmask, LogL level);
void VLog(LogS src, LogL level, const char* format, va_list args);
void Log(LogS src, LogL level, const char* format, ...);

enum class Stage {
    Default, Init, ReturnToMenu, NoDisc,
    SpinupDisc, SpinupDiscNoCache, DiscError, ReadDisc
};

extern Queue<Stage> events;

}

