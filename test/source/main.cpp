#include "irse.h"

#include "dvd.h"
#include "File.hpp"
#include <wiiuse/wpad.h>
#include "util.hpp"
LIBOGC_SUCKS_BEGIN
#include <ogc/machine/processor.h>
LIBOGC_SUCKS_END

#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <mutex>

#include "Boot.hpp"
#include "GlobalsConfig.hpp"
#include "IOSBoot.hpp"

using namespace irse;

struct {
    s32 verbosity;
    bool useCache;
} state;

Queue<Stage> irse::events;

static Thread mainThread;

static struct {
    void *xfb = NULL;
    GXRModeObj *rmode = NULL;
} display;

static s32 Loop(void* arg);

static Stage stDefault(Stage from);
static Stage stInit(Stage from);
static Stage stReturnToMenu(Stage from);
static Stage stNoDisc(Stage from);
static Stage stSpinupDisc(Stage from);
static Stage stSpinupDiscNoCache(Stage from);
static Stage stDiscError(Stage from);
static Stage stReadDisc(Stage from);

static constexpr std::array<const char*, 5> logSources = {
    "Core", "DVD", "Loader", "Payload", "IOS" };
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

void irse::VLog(LogS src, LogL level, const char* format, va_list args)
{
    DASSERT(logInit);
    u32 slvl = static_cast<u32>(level);
    u32 schan = static_cast<u32>(src);
    ASSERT(slvl < logColors.size());
    ASSERT(schan < logSources.size());

    if (level != LogL::ERROR) {
        if (!(logMask & (1 << schan)))
            return;
        if (slvl < logLevel)
            return;
    }
    {
        std::unique_lock<Mutex> lock(logMutex);
	    vsnprintf(&logBuffer[0], 256, format, args);

        // TODO: Skip newline at the end of format string
        printf("%s[%s] %s\n\x1b[37;1m",
            logColors[slvl], logSources[schan], logBuffer.data());
    }
}

void irse::Log(LogS src, LogL level, const char* format, ...)
{
    va_list args;
	va_start(args, format);
    VLog(src, level, format, args);
    va_end(args);
}

/* 
 * Checks to see if we have any events, like shutdown commands.
 * Returns to the previous stage if none, after waiting for some amount
 * of time.
 */
static Stage stDefault(Stage from)
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

extern u8 saoirse_ios_elf[];
extern u32 saoirse_ios_elf_size;

static Stage stInit([[maybe_unused]] Stage from)
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

    irse::Log(LogS::Core, LogL::INFO, "Starting up IOS...");
    const s32 ret = IOSBoot::Launch(saoirse_ios_elf, saoirse_ios_elf_size);
    irse::Log(LogS::Core, LogL::INFO, "IOS Launch result: %d", ret);
    new IOSBoot::Log();

    /* Wait for DI to startup */
    usleep(2000);
    DVD::Init();

    /* Test filesystem
     * note this will fail if you don't have the test file lol */
    file::init();
    file fl("test_file.txt", FA_READ);
    if (fl.result() != FResult::FR_OK) {
        irse::Log(LogS::Core, LogL::INFO, "test f_open: %d", fl.result());
        abort();
    }

    static char str[sizeof("This is a test file.")];
    u32 read;
    FResult fret = fl.read(str, sizeof(str) - 1, read);
    if (fret != FResult::FR_OK) {
        irse::Log(LogS::Core, LogL::ERROR, "test f_read: %d", fret);
        abort();
    }

    irse::Log(LogS::Core, LogL::INFO, "str: %s", str);

    if (DVD::IsInserted()) {
        return Stage::SpinupDisc;
    }
    return Stage::NoDisc;
}

static Stage stReturnToMenu([[maybe_unused]] Stage from)
{
    irse::Log(LogS::Core, LogL::WARN, "Exiting...");

    if (DVD::IsInserted()) {
        DVD::ResetDrive(false);
    }
    exit(0);
    /* Should never reach here */
    return Stage::ReturnToMenu;
}

static Stage stNoDisc(Stage from)
{
    if (from != Stage::Default) {
        irse::Log(LogS::Core, LogL::WARN, "Waiting for disc insert...");
    }

    if (!DVD::IsInserted()) {
        return Stage::Default;
    }
    return Stage::SpinupDiscNoCache;
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
    if (ret != DiErr::OK)
        return false;

    ret = DVD::ReadDiskID(reinterpret_cast<DVD::DiskID*>(MEM1_BASE));
    return ret == DiErr::OK;
}

static Stage stSpinupDisc([[maybe_unused]] Stage from)
{
    if (!startupDrive())
        return Stage::DiscError;

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

    return Stage::ReadDisc;
}

static Stage stSpinupDiscNoCache([[maybe_unused]] Stage from)
{
    if (!startupDrive())
        return Stage::DiscError;

    DiErr ret = DVD::ReadDiskID(reinterpret_cast<DVD::DiskID*>(MEM1_BASE));
    if (ret != DiErr::OK) {
        irse::Log(LogS::Core, LogL::ERROR, "DVD::ReadDiskID returned %s",
            DVDLow::PrintErr(ret));
        return Stage::DiscError;
    }

    return Stage::ReadDisc;
}

static Stage stDiscError([[maybe_unused]] Stage from)
{
    if (from != Stage::Default) {
        DVD::ResetDrive(false);
        irse::Log(LogS::Core, LogL::WARN, "Disc error, waiting for eject...");
        /* Drive reset will confuse the cover status at first */
        return Stage::Default;
    }

    if (DVD::IsInserted()) {
        return Stage::Default;
    }
    return Stage::NoDisc;
}

static Stage stReadDisc([[maybe_unused]] Stage from)
{
    irse::Log(LogS::Core, LogL::INFO,
        "DiskID: %.6s", reinterpret_cast<char*>(MEM1_BASE));

    WPAD_Shutdown();

    static Apploader loader;
    auto main = loader.load();

    SetupGlobals(0);
    
    // TODO: Proper shutdown
    SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
    IRQ_Disable();
    main();
    /* Unreachable! */
    abort();
}

static s32 Loop([[maybe_unused]] void* arg)
{
    Stage stage = Stage::Init;
    Stage prev = Stage::Default;
    Stage next = Stage::ReturnToMenu;

    while (1) {
        switch (stage) {
#define STAGE_CASE(name) case Stage::name: next =  st ## name(prev); break
            STAGE_CASE(Default);
            STAGE_CASE(Init);
            STAGE_CASE(ReturnToMenu);
            STAGE_CASE(NoDisc);
            STAGE_CASE(SpinupDisc);
            STAGE_CASE(SpinupDiscNoCache);
            STAGE_CASE(DiscError);
            STAGE_CASE(ReadDisc);
#undef STAGE_CASE
        }
        prev = stage;
        stage = next;
    }
}

s32 main([[maybe_unused]] s32 argc, [[maybe_unused]] char** argv)
{
    // TODO: Do IOS reload at the right time (just before launch)
    IOS_ReloadIOS(36);

    /* Initialize Wii Remotes */
    WPAD_Init();

    LWP_SetThreadPriority(LWP_GetSelf(), 50);
    mainThread.create(Loop, nullptr, nullptr, 1024, LWP_PRIO_HIGHEST - 20);
    
    while (1) {
        usleep(16000);
        WPAD_ScanPads();

        s32 down = WPAD_ButtonsDown(0);
        if (down & WPAD_BUTTON_HOME) {
            events.send(Stage::ReturnToMenu);
            break;
        }
    }
    while (1) { }
    return 0;
}
