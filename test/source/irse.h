#pragma once

#include <stdio.h>
#include <gccore.h>
#include "os.h"
#include <mutex>

enum class LogS {
    Core, DVD,
};
enum class LogL { INFO, WARN, ERROR };

namespace irse
{

void LogConfig(u32 srcmask, LogL level);
void Log(LogS src, LogL level, const char* format, ...);

enum class Stage {
    stDefault, stInit, stReturnToMenu, stNoDisc, stSpinupDisc, stSpinupDiscNoCache, stDiscError, stReadDisc
};

extern Queue<Stage> events;

}

