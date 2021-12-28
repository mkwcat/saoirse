#include "irse.h"

#include "dvd.h"
#include <sdcard.h>
#include <util.h>
LIBOGC_SUCKS_BEGIN
#include <ogc/machine/processor.h>
#include <wiiuse/wpad.h>
LIBOGC_SUCKS_END

#include "Apploader.hpp"
#include "GlobalsConfig.hpp"
#include "IODeviceManager.hpp"
#include "IOSBoot.hpp"
#include "UIManager.h"
#include "arch.h"
#include <cstring>
#include <disk.h>
#include <ff.h>
#include <mutex>
#include <stdio.h>
#include <unistd.h>

using namespace irse;

Queue<Stage> irse::events;

static struct {
    void* xfb = NULL;
    GXRModeObj* rmode = NULL;
} display;

static constexpr std::array<const char*, 8> logSources = {
    "Core", "DVD", "Loader", "Payload", "IOS", "FST", "DiskIO", "IOMgr"};
static constexpr std::array<const char*, 3> logColors = {
    "\x1b[37;1m", "\x1b[33;1m", "\x1b[31;1m"};
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
        printf("%s[%s] %s\n\x1b[37;1m", logColors[slvl], logSources[schan],
               logBuffer.data());
    }
}

void irse::Log(LogS src, LogL level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    VLog(src, level, format, args);
    va_end(args);
}

static inline bool startupDrive()
{
    // If ReadDiskID succeeds here, that means the drive is already started
    DiErr ret = DVD::ReadDiskID(reinterpret_cast<DVD::DiskID*>(MEM1_BASE));
    if (ret == DiErr::OK) {
        irse::Log(LogS::Core, LogL::INFO, "Drive is already spinning");
        return true;
    }
    if (ret != DiErr::DriveError)
        return false;

    // Drive is not spinning
    irse::Log(LogS::Core, LogL::INFO, "Spinning up drive...");
    ret = DVD::ResetDrive(true);
    if (ret != DiErr::OK)
        return false;

    ret = DVD::ReadDiskID(reinterpret_cast<DVD::DiskID*>(MEM1_BASE));
    return ret == DiErr::OK;
}

s32 main([[maybe_unused]] s32 argc, [[maybe_unused]] char** argv)
{
    /* Initialize video and the debug console */
    VIDEO_Init();
    display.rmode = VIDEO_GetPreferredMode(NULL);
    display.xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(display.rmode));
    console_init(display.xfb, 20, 20, display.rmode->fbWidth,
                 display.rmode->xfbHeight,
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

    extern const char data_ar[];
    extern const char data_ar_end[];
    Arch::sInstance = new Arch(data_ar, data_ar_end - data_ar);

    // Launch Saoirse IOS
    IOSBoot::LaunchSaoirseIOS();

    // Start "UI" thread
    new Thread(UIManager::threadEntry, nullptr, nullptr, 0x8000, 80);
    // Start IODeviceManager thread
    new Thread(IODeviceManager::threadEntry, nullptr, nullptr, 0x8000, 80);

    // TODO move this to like a page based UI system or something
    DVD::Init();
    if (!startupDrive()) {
        abort();
    }

    Queue<int> waitDestroy(1);

    EntryPoint entry;
    Apploader* apploader = new Apploader(&entry);
    apploader->start(&waitDestroy);

    bool result = waitDestroy.receive();
    if (result != 0) {
        irse::Log(LogS::Core, LogL::ERROR, "Apploader aborted");
        abort();
    }

    SDCard::Shutdown();
    DVD::Deinit();

    IOSBoot::Log::sInstance->startGameIOS();

    delete IOSBoot::Log::sInstance;

    VIDEO_SetBlack(true);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    SetupGlobals(0);

    // TODO: Proper shutdown
    SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
    IRQ_Disable();

    entry();
    /* Unreachable! */
    abort();
    //LWP_SuspendThread(LWP_GetSelf());
}
