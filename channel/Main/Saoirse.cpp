// Saoirse.cpp
//   Written by Palapeli
//
// SPDX-License-Identifier: MIT

#include "Saoirse.hpp"
#include "Arch.hpp"
#include "GlobalsConfig.hpp"
#include "IOSBoot.hpp"
#include <Apploader/Apploader.hpp>
#include <DVD/DI.hpp>
#include <Debug/Log.hpp>
#include <Main/LaunchState.hpp>
#include <Patch/Codehandler.hpp>
#include <Patch/PatchList.hpp>
#include <Patch/Riivolution.hpp>
#include <System/ISFS.hpp>
#include <System/Util.h>
#include <UI/BasicUI.hpp>
#include <UI/Input.hpp>
#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
LIBOGC_SUCKS_BEGIN
#include <ogc/context.h>
#include <ogc/exi.h>
#include <ogc/machine/processor.h>
#include <wiiuse/wpad.h>
LIBOGC_SUCKS_END

bool RTCRead(u32 offset, u32* value)
{
    if (EXI_Lock(EXI_CHANNEL_0, EXI_DEVICE_1, NULL) == 0)
        return false;
    if (EXI_Select(EXI_CHANNEL_0, EXI_DEVICE_1, EXI_SPEED8MHZ) == 0) {
        EXI_Unlock(EXI_CHANNEL_0);
        return false;
    }

    bool ret = true;
    if (EXI_Imm(EXI_CHANNEL_0, &offset, 4, EXI_WRITE, NULL) == 0)
        ret = false;
    if (EXI_Sync(EXI_CHANNEL_0) == 0)
        ret = false;
    if (EXI_Imm(EXI_CHANNEL_0, value, 4, EXI_READ, NULL) == 0)
        ret = false;
    if (EXI_Sync(EXI_CHANNEL_0) == 0)
        ret = false;
    if (EXI_Deselect(EXI_CHANNEL_0) == 0)
        ret = false;
    EXI_Unlock(EXI_CHANNEL_0);

    return ret;
}

bool RTCWrite(u32 offset, u32 value)
{
    if (EXI_Lock(EXI_CHANNEL_0, EXI_DEVICE_1, NULL) == 0)
        return false;
    if (EXI_Select(EXI_CHANNEL_0, EXI_DEVICE_1, EXI_SPEED8MHZ) == 0) {
        EXI_Unlock(EXI_CHANNEL_0);
        return false;
    }

    // Enable write mode
    offset |= 0x80000000;

    bool ret = true;
    if (EXI_Imm(EXI_CHANNEL_0, &offset, 4, EXI_WRITE, NULL) == 0)
        ret = false;
    if (EXI_Sync(EXI_CHANNEL_0) == 0)
        ret = false;
    if (EXI_Imm(EXI_CHANNEL_0, &value, 4, EXI_WRITE, NULL) == 0)
        ret = false;
    if (EXI_Sync(EXI_CHANNEL_0) == 0)
        ret = false;
    if (EXI_Deselect(EXI_CHANNEL_0) == 0)
        ret = false;
    EXI_Unlock(EXI_CHANNEL_0);

    return ret;
}

// Re-enables holding the power button to turn off the console on vWii
bool WiiUEnableHoldPower()
{
    // RTC_CONTROL1 |= 4COUNT_EN

    u32 flags = 0;
    bool ret = RTCRead(0x21000D00, &flags);
    if (!ret)
        return false;

    ret = RTCWrite(0x21000D00, flags | 1);
    if (!ret)
        return false;

    return true;
}

static void PIErrorHandler(
  [[maybe_unused]] u32 nIrq, [[maybe_unused]] void* pCtx)
{
    // u32 cause = read32(0x0C003000); // INTSR
    write32(0x0C003000, 1); // Reset

    // PRINT(Core, ERROR, "PI error occurred!  Cause: 0x%04X", cause);
}

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

void abort()
{
    // *(u32*)0x12345678 = 0;

    LIBOGC_SUCKS_BEGIN
    u32 lr = mfspr(8);
    LIBOGC_SUCKS_END

    PRINT(Core, ERROR, "Abort called. LR = 0x%08X\n", lr);

    sleep(2);
    exit(0);

    // If this somehow returns then halt.
    IRQ_Disable();
    while (1) {
    }
}

static s32 UIThreadEntry([[maybe_unused]] void* arg)
{
    BasicUI::sInstance->Loop();
    return 0;
}

// PatchList patchList;
EntryPoint entry;

void LaunchGame()
{
    VIDEO_SetBlack(true);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    SetupGlobals(0);

    // patchList.ApplyPatches();

    // TODO: Proper shutdown
    SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
    IRQ_Disable();

    entry();
    /* Unreachable! */
    abort();
    // LWP_SuspendThread(LWP_GetSelf());
}

s32 main([[maybe_unused]] s32 argc, [[maybe_unused]] char** argv)
{
    // Properly handle PI errors
    IRQ_Request(IRQ_PI_ERROR, PIErrorHandler, nullptr);
    __UnmaskIrq(IM_PI_ERROR);

    // This is a nice thing to enable for development, but we should probably
    // leave it disabled for the end user, unless we can figure out why it was
    // disabled in the first place.
    WiiUEnableHoldPower();

    // Start of the game apploader
    SYS_SetArena1Hi((void*) 0x81200000);

    IOSBoot::Init();

    Input::sInstance = new Input();
    BasicUI::sInstance = new BasicUI();
    BasicUI::sInstance->InitVideo();
    new Thread(UIThreadEntry, nullptr, nullptr, 0x1000, 80);

    PRINT(Core, WARN, "Debug console initialized");
    VIDEO_WaitVSync();

    // Setup main data archive
    extern const char data_ar[];
    extern const char data_ar_end[];
    Arch::sInstance = new Arch(data_ar, data_ar_end - data_ar);

    // TODO: Manage this instance
    new Riivolution();

    // Launch Saoirse IOS
    IOSBoot::LaunchSaoirseIOS();

    PRINT(Core, INFO, "Send start game IOS request!");
    IOSBoot::IPCLog::sInstance->startGameIOS();

    DI::sInstance = new DI;

    // TODO move this to like a page based UI system or something
    if (!startupDrive()) {
        PRINT(Core, ERROR, "Startup drive failed");
        abort();
    }

    LaunchState::Get()->DiscInserted.state = true;
    LaunchState::Get()->DiscInserted.available = true;
    LaunchState::Get()->ReadDiscID.state = true;
    LaunchState::Get()->ReadDiscID.available = true;

    usleep(32000);

    Queue<int> waitDestroy(1);

    Apploader* apploader = new Apploader(&entry);
    apploader->start(&waitDestroy);

    bool result = waitDestroy.receive();
    if (result != 0) {
        PRINT(Core, ERROR, "Apploader aborted");
        abort();
    }
#if 0
    Codehandler::ImportCodehandler(&patchList);

    u32 gctSize;
    const u8* gct = (u8*)Arch::getFileStatic("RMCP01.gct", &gctSize);

    Codehandler::ImportGCT(&patchList, gct, gct + gctSize);
    patchList.ImportPokeBranch(0x801AAAA0, 0x800018A8);
#endif

    delete DI::sInstance;

    delete IOSBoot::IPCLog::sInstance;
    PRINT(Core, INFO, "Wait for UI...");

    LaunchState::Get()->LaunchReady.state = true;
    LaunchState::Get()->LaunchReady.available = true;

    LWP_SuspendThread(LWP_GetSelf());
}
