#include "irse.h"

#include "dvd.h"
#include <gccore.h>
#include <gcutil.h>
#include <wiiuse/wpad.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


namespace irse
{

struct {
    s32 verbosity;
    bool useCache;
    u64 waitTime;
} state;

Queue<Stage> events;

s32 Loop(void* arg);

Stage stDefault(Stage from);
Stage stInit(Stage from);
Stage stReturnToMenu(Stage from);
Stage stNoDisc(Stage from);
Stage stSpinupDisc(Stage from);
Stage stSpinupDiscNoCache(Stage from);
Stage stDiscError(Stage from);
Stage stReadDisc(Stage from);

}

static Thread mainThread;

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

/* 
 * Checks to see if we have any events, like shutdown commands.
 * Returns to the previous stage if none, after waiting for some amount
 * of time.
 */
irse::Stage irse::stDefault(Stage from)
{
    Stage next;
    if (!events.tryreceive(next)) {
        //! No event
        /* Wait 32 ms */
        usleep(32000);
        return from;
    } else {
        //! Received event
        return next;
    }
}

irse::Stage irse::stInit(Stage from)
{
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

    if (DVD::IsInserted()) {
        return Stage::stSpinupDisc;
    }
    return Stage::stNoDisc;
}

irse::Stage irse::stReturnToMenu(Stage from)
{
    if (DVD::IsInserted()) {
        DVD::ResetDrive(false);
    }
    exit(0);
    /* Should never reach here */
    return Stage::stReturnToMenu;
}

irse::Stage irse::stNoDisc(Stage from)
{
    if (from != Stage::stDefault) {
        irse::Log(LogS::Core, LogL::WARN, "Waiting for disc insert...");
    }

    if (!DVD::IsInserted()) {
        return Stage::stDefault;
    }
    return Stage::stSpinupDiscNoCache;
}

static inline
bool startupDrive()
{
    /* If ReadDiskID succeeds here, that means the drive is already started */
    DiErr ret = DVD::ReadDiskID(reinterpret_cast<DVD::DiskID*>(MEM1_BASE));
    if (ret == DiErr::OK) {
        irse::Log(LogS::Core, LogL::INFO, "Drive is already spinning");
        return true;
    }
    if (ret != DiErr::DriveError)
        return false;

    /* Drive is not spinning */
    irse::Log(LogS::Core, LogL::INFO, "Spinning up drive...");
    ret = DVD::ResetDrive(true);
    return ret == DiErr::OK;
}

irse::Stage irse::stSpinupDisc(Stage from)
{
    if (!startupDrive())
        return Stage::stDiscError;

    /* Initialize the system menu cache.dat */
    state.useCache = DVD::OpenCacheFile();
    if (state.useCache) {
        DVD::DiskID cached ATTRIBUTE_ALIGN(32);
        DiErr ret = DVD::ReadCachedDiskID(&cached);

        if (ret == DiErr::OK && memcmp(MEM1_BASE, &cached.gameID, 6)) {
            irse::Log(LogS::Core, LogL::WARN,
                "Cached diskid does not equal real diskid");
            state.useCache = false;
        }
    }

    return Stage::stReadDisc;
}

irse::Stage irse::stSpinupDiscNoCache(Stage from)
{
    if (!startupDrive())
        return Stage::stDiscError;

    DiErr ret = DVD::ReadDiskID(reinterpret_cast<DVD::DiskID*>(MEM1_BASE));
    if (ret != DiErr::OK) {
        irse::Log(LogS::Core, LogL::ERROR, "DVD::ReadDiskID returned %s",
            DVDLow::PrintErr(ret));
        return Stage::stDiscError;
    }

    return Stage::stReadDisc;
}

irse::Stage irse::stDiscError(Stage from)
{
    if (from != Stage::stDefault) {
        DVD::ResetDrive(false);
        irse::Log(LogS::Core, LogL::WARN, "Disc error, waiting for eject...");
        /* Drive reset will throw off the cover status at first */
        return Stage::stDefault;
    }

    if (DVD::IsInserted()) {
        return Stage::stDefault;
    }
    return Stage::stNoDisc;
}

irse::Stage irse::stReadDisc(Stage from)
{
    irse::Log(LogS::Core, LogL::INFO,
        "DiskID: %.6s", reinterpret_cast<char*>(MEM1_BASE));

    /* Next stage not implemented yet so just wait for disc eject */
    return Stage::stDiscError;
}

s32 irse::Loop(void* arg)
{
    Stage stage = Stage::stInit;
    Stage prev = Stage::stDefault;
    Stage next = Stage::stReturnToMenu;
    state.waitTime = 32000;

    while (1) {
        switch (stage) {
#define STAGE_CASE(name) case Stage::name: next = name(prev); break
            STAGE_CASE(stDefault);
            STAGE_CASE(stInit);
            STAGE_CASE(stReturnToMenu);
            STAGE_CASE(stNoDisc);
            STAGE_CASE(stSpinupDisc);
            STAGE_CASE(stSpinupDiscNoCache);
            STAGE_CASE(stDiscError);
            STAGE_CASE(stReadDisc);
#undef STAGE_CASE
        }
        prev = stage;
        stage = next;
    }
}

s32 main(s32 argc, char** argv)
{
    /* Initialize controllers */
    WPAD_Init();

    LWP_SetThreadPriority(LWP_GetSelf(), 50);
    mainThread.create(irse::Loop, nullptr, nullptr, 1024, LWP_PRIO_HIGHEST - 20);
    
    while (1) {
        usleep(16000);
        WPAD_ScanPads();

        s32 down = WPAD_ButtonsDown(0);
        if (down & WPAD_BUTTON_HOME) {
            irse::events.send(irse::Stage::stReturnToMenu);
            break;
        }
    }
    while (1) { }
    return 0;
}
