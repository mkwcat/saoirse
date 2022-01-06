#include "Arch.hpp"
#include "GlobalsConfig.hpp"
#include "IODeviceManager.hpp"
#include "IOSBoot.hpp"
#include "UIManager.hpp"
#include <Apploader/Apploader.hpp>
#include <DVD/DI.hpp>
#include <Debug/Log.hpp>
#include <Disk/SDCard.hpp>
#include <System/Util.hpp>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
LIBOGC_SUCKS_BEGIN
#include <ogc/machine/processor.h>
#include <wiiuse/wpad.h>
LIBOGC_SUCKS_END

static struct {
    void* xfb = NULL;
    GXRModeObj* rmode = NULL;
} display;

static inline bool startupDrive()
{
    // If ReadDiskID succeeds here, that means the drive is already started
    DI::DIError ret =
        DI::sInstance->ReadDiskID(reinterpret_cast<DI::DiskID*>(MEM1_BASE));
    if (ret == DI::DIError::OK) {
        PRINT(Core, INFO, "Drive is already spinning");
        return true;
    }
    if (ret != DI::DIError::Drive)
        return false;

    // Drive is not spinning
    PRINT(Core, INFO, "Spinning up drive...");
    ret = DI::sInstance->Reset(true);
    if (ret != DI::DIError::OK)
        return false;

    ret = DI::sInstance->ReadDiskID(reinterpret_cast<DI::DiskID*>(MEM1_BASE));
    return ret == DI::DIError::OK;
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

    PRINT(Core, WARN, "Debug console initialized");
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
    DI::sInstance = new DI;
    if (!startupDrive()) {
        abort();
    }

    Queue<int> waitDestroy(1);

    EntryPoint entry;
    Apploader* apploader = new Apploader(&entry);
    apploader->start(&waitDestroy);

    bool result = waitDestroy.receive();
    if (result != 0) {
        PRINT(Core, ERROR, "Apploader aborted");
        abort();
    }

    SDCard::Shutdown();
    delete DI::sInstance;

    PRINT(Core, INFO, "Send start game IOS request!");
    IOSBoot::IPCLog::sInstance->startGameIOS();
    delete IOSBoot::IPCLog::sInstance;

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
    // LWP_SuspendThread(LWP_GetSelf());
}
