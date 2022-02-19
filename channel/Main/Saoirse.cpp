// Saoirse.cpp
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
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
#include <System/Util.h>
#include <UI/BasicUI.hpp>
#include <UI/Input.hpp>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
LIBOGC_SUCKS_BEGIN
#include <ogc/context.h>
#include <ogc/machine/processor.h>
#include <wiiuse/wpad.h>
LIBOGC_SUCKS_END

static void PIErrorHandler([[maybe_unused]] u32 nIrq,
                           [[maybe_unused]] void* pCtx)
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

PatchList patchList;
EntryPoint entry;

void LaunchGame()
{
    PRINT(Core, INFO, "Send start game IOS request!");
    IOSBoot::IPCLog::sInstance->startGameIOS();
    delete IOSBoot::IPCLog::sInstance;

    VIDEO_SetBlack(true);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    SetupGlobals(0);

    patchList.ApplyPatches();

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

    DI::sInstance = new DI;

    // Launch Saoirse IOS
    IOSBoot::LaunchSaoirseIOS();

    // TODO move this to like a page based UI system or something
    if (!startupDrive()) {
        abort();
    }

    LaunchState::Get()->DiscInserted.state = true;
    LaunchState::Get()->DiscInserted.available = true;
    LaunchState::Get()->ReadDiscID.state = true;
    LaunchState::Get()->ReadDiscID.available = true;

    Queue<int> waitDestroy(1);

    Apploader* apploader = new Apploader(&entry);
    apploader->start(&waitDestroy);

    bool result = waitDestroy.receive();
    if (result != 0) {
        PRINT(Core, ERROR, "Apploader aborted");
        abort();
    }

    Codehandler::ImportCodehandler(&patchList);

    u32 gctSize;
    const u8* gct = (u8*)Arch::getFileStatic("RMCP01.gct", &gctSize);

    Codehandler::ImportGCT(&patchList, gct, gct + gctSize);
    patchList.ImportPokeBranch(0x801AAAA0, 0x800018A8);

    delete DI::sInstance;

    LaunchState::Get()->LaunchReady.state = true;
    LaunchState::Get()->LaunchReady.available = true;

    PRINT(Core, INFO, "Wait for UI start game request!");

    LWP_SuspendThread(LWP_GetSelf());
}
