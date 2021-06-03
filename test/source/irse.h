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

void Loop();

enum class Stage {
    stInit, stNoDisc, stSpinupDisc, stSpinupDiscNoCache, stDiscError, stReadDisc
};
void stInit();
void stNoDisc();
void stSpinupDisc();
void stSpinupDiscNoCache();
void stDiscError();
void stReadDisc();

}

