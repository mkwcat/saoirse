#pragma once

#include <util.h>
#include <stdlib.h>
#include <stdarg.h>
#include <os.h>

enum class LogS {
    Core, DVD, Loader, Payload, IOS
};
enum class LogL { INFO, WARN, ERROR };

namespace irse
{

// TODO: Config logging level for individual sources
void LogConfig(u32 srcmask, LogL level);
void VLog(LogS src, LogL level, const char* format, va_list args);
void Log(LogS src, LogL level, const char* format, ...);

enum class Stage {
    Default, Init, Wait, ReturnToMenu,

    DiscInsert, DiscEject, DiscError, ReadDisc,

    SDInsert, SDEject
};

extern Queue<Stage> events;

}

