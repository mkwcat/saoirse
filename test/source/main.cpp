#include "irse.h"

#include "dvd.h"
#include <gccore.h>
#include <wiiuse/wpad.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static struct {
    s32 verbosity;
    irse::Stage stage;
} state;

static struct {
    void *xfb = NULL;
    GXRModeObj *rmode = NULL;
} display;

static constexpr std::array<const char*, 2> logSources = {
    "Core", "DVD" };
static constexpr std::array<const char*, 3> logColors = {
    "\x1b[37;1m", "\x1b[33;1m", "\x1b[31;1m" };
static std::array<char, 256> logBuffer;
static Mutex logMutex(-1);
static u32 logMask;
static u32 logLevel;
static bool logInit = false;

void irse::LogConfig(u32 srcmask, LogL level)
{
    logMask = srcmask;
    logLevel = static_cast<u32>(level);
    DASSERT(logLevel < logColors.size());
    new (&logMutex) Mutex();
    logInit = true;
}

void irse::Log(LogS src, LogL level, const char* format, ...)
{
    DASSERT(logInit);
    u32 slvl = static_cast<u32>(level);
    u32 schan = static_cast<u32>(src);
    ASSERT(slvl < logColors.size());
    ASSERT(schan < logSources.size());

    if (!(logMask & (1 << schan)))
        return;
    if (slvl < logLevel)
        return;
    {
        std::unique_lock<Mutex> lock(logMutex);
        va_list args;
	    va_start(args, format);
	    vsnprintf(&logBuffer[0], 256, format, args);
	    va_end(args);

        printf("%s[%s] %s\n\x1b[37;1m",
            logColors[slvl], logSources[schan], logBuffer.data());
    }
}

static DVDLow::DVDCommand* motor;

void irse::stInit()
{
    /* Initialize controllers */
    WPAD_Init();

    /* Initialize video and the debug console */
    VIDEO_Init();
    display.rmode = VIDEO_GetPreferredMode(NULL);
    display.xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(display.rmode));
    console_init(display.xfb, 20, 20,
                 display.rmode->fbWidth, display.rmode->xfbHeight,
                 display.rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(display.rmode);
    VIDEO_SetNextFramebuffer(display.xfb);
    VIDEO_SetBlack(0);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (display.rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();
    printf("\x1b[2;0H");

    irse::LogConfig(0xFFFFFFFF, LogL::INFO);
    irse::Log(LogS::Core, LogL::WARN, "Debug console initialized");
    VIDEO_WaitVSync();
    DVD::Init();
    motor = new DVDLow::DVDCommand;

    if (DVD::IsInserted()) {
        state.stage = Stage::stSpinupDisc;
    } else {
        state.stage = Stage::stNoDisc;
    }
}

void irse::stNoDisc()
{
    irse::Log(LogS::Core, LogL::INFO, "Waiting for disc insert...");

    while (!DVD::IsInserted()) {
        usleep(32000);
    }

    state.stage = Stage::stSpinupDiscNoCache;
}

void irse::stSpinupDisc()
{
    /* If ReadDiskID succeeds here, that means the drive is already started */
    DiErr ret = DVD::ReadDiskID(reinterpret_cast<DVD::DiskID*>(MEM1_BASE));
    if (ret == DiErr::OK) {
        state.stage = Stage::stReadDisc;
        return;
    }

    if (ret != DiErr::DriveError) {
        state.stage = Stage::stDiscError;
        return;
    }

    /* ReadDiskID returned DriveError, so we have to spinup the drive */
    state.stage = Stage::stSpinupDiscNoCache;
}

void irse::stSpinupDiscNoCache()
{
    DiErr ret = DVD::ResetDrive();
    if (ret != DiErr::OK) {
        state.stage = Stage::stDiscError;
        return;
    }

    ret = DVD::ReadDiskID(reinterpret_cast<DVD::DiskID*>(MEM1_BASE));
    if (ret != DiErr::OK) {
        irse::Log(LogS::Core, LogL::ERROR, "DVD::ReadDiskID returned %s",
            DVDLow::PrintErr(ret));
        state.stage = Stage::stDiscError;
        return;
    }

    state.stage = Stage::stReadDisc;
}

void irse::stDiscError()
{
    irse::Log(LogS::Core, LogL::INFO, "Waiting for disc eject...");

    while (DVD::IsInserted()) {
        usleep(32000);
    }

    state.stage = Stage::stNoDisc;
}

void irse::stReadDisc()
{
    irse::Log(LogS::Core, LogL::INFO,
        "DiskID: %.6s", reinterpret_cast<char*>(MEM1_BASE));

    /* Next stage not implemented yet so just wait for disc eject */
    state.stage = Stage::stDiscError;
}

void irse::Loop()
{
    state.stage = Stage::stInit;

    while (1) {
        switch (state.stage) {
#define STAGE_CASE(name) case Stage::name: name(); break
            STAGE_CASE(stInit);
            STAGE_CASE(stNoDisc);
            STAGE_CASE(stSpinupDisc);
            STAGE_CASE(stSpinupDiscNoCache);
            STAGE_CASE(stDiscError);
            STAGE_CASE(stReadDisc);
#undef STAGE_CASE
        }
    }
}

s32 main(s32 argc, char** argv)
{
    irse::Loop();
    return 0;
}









