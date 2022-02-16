// Apploader.cpp - Wii disc apploader
//   Written by riidefi
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#include "Apploader.hpp"
#include "AppPayload.hpp"
#include "GlobalsConfig.hpp"
#include "IOSBoot.hpp"
#include <DVD/DI.hpp>
#include <Debug/Log.hpp>
#include <System/Util.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <stdint.h>
#include <string.h>
#include <thread>
#include <time.h>
#include <unistd.h>
LIBOGC_SUCKS_BEGIN
#include <ogc/cache.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
LIBOGC_SUCKS_END

void Apploader::taskEntry()
{
    *m_resultOut = load();
}

#if 0
static void DebugPause() {
    using namespace std;
    std::this_thread::sleep_for(2s);
}
#else
static void DebugPause()
{
}
#endif

static void UnencryptedRead(void* dst, u32 len, u32 ofs)
{
    const auto result = DI::sInstance->UnencryptedRead(dst, len, ofs);

    if (result != DI::DIError::OK) {
        PRINT(Loader, ERROR, "Failed to execute unencrypted read: %s",
              DI::PrintError(result));
        return;
    }
}

static void EncryptedRead(void* dst, u32 len, u32 ofs)
{
    const auto result = DI::sInstance->Read(dst, len, ofs);

    if (result != DI::DIError::OK) {
        PRINT(Loader, ERROR, "Failed to execute encrypted read: %s",
              DI::PrintError(result));
        return;
    }
}

class PayloadManager
{
public:
    PayloadManager(const ApploaderInfo& info) : mPayload(info)
    {
    }

    void loadSegments(TaskThread* breakThread)
    {
        while (true) {
            if (breakThread != nullptr)
                breakThread->taskBreak();

            const std::optional<AppPayload::CopyCommand> copy_cmd =
                mPayload.popCopyCommand();
            if (!copy_cmd.has_value())
                break;

            if (breakThread != nullptr)
                breakThread->taskBreak();

            EncryptedRead(copy_cmd->dest, copy_cmd->length,
                          round_down(copy_cmd->offset, 4));
        }
    }

    EntryPoint getEntrypoint() const
    {
        return mPayload.get_entrypoint();
    }

private:
    AppPayload mPayload;
};

EntryPoint Apploader::load(
    // TODO: If we want to shrink the FST to fit some code a-la CTGP.
    [[maybe_unused]] int fst_expand)
{
    openBootPartition(&m_meta);

    taskBreak();

    const ApploaderInfo app_info = readAppInfo();
    dumpAppInfo(app_info);
    DebugPause();

    taskBreak();

    // XXX Partition must be open
    PayloadManager payload(app_info);
    DebugPause();
    settime(secs_to_ticks(time(NULL) - 946684800));
    DebugPause();
    payload.loadSegments(this);

    taskBreak();

    DebugPause();
    return payload.getEntrypoint();
}

void Apploader::openBootPartition(ES::TMDFixed<512>* outMeta)
{
    const Volume main_volume = readVolumes()[0];

    taskBreak();

    const auto partitions = readPartitions(main_volume);

    taskBreak();

    const Partition* boot_partition =
        findBootPartition(main_volume, partitions);
    if (boot_partition == nullptr) {
        PRINT(Loader, ERROR, "Failed to find boot partition");
        taskAbort();
    }
    PRINT(Loader, INFO, "Boot partition: %p",
          reinterpret_cast<const void*>(boot_partition));

    taskBreak();

    openPartition(*boot_partition, outMeta);
}

void Apploader::dumpAppInfo(const ApploaderInfo& app_info)
{
    auto mem_dump = [](std::span<u32> mem) {
        PRINT(Loader, INFO, "MEMORY DUMP");
        for (std::size_t i = 0; i < mem.size(); i += 4) {
            PRINT(Loader, INFO, "%p: %08X %08X %08X %08X",
                  reinterpret_cast<void*>(&mem[i]), mem[i], mem[i + 1],
                  mem[i + 2], mem[i + 3]);
        }
    };
    mem_dump({(u32*)&app_info, 32});
}

ApploaderInfo Apploader::readAppInfo()
{
    static ApploaderInfo app_info ATTRIBUTE_ALIGN(32);

    PRINT(Loader, INFO, "Reading apploader info..");
    EncryptedRead(&app_info, sizeof(app_info), 0x2440 / 4);
    DebugPause();
    return app_info;
}

void Apploader::openPartition(const Partition& partition,
                              ES::TMDFixed<512>* outMeta)
{
    const auto result = DI::sInstance->OpenPartition(partition.offset, outMeta);

    if (result != DI::DIError::OK) {
        PRINT(Loader, ERROR, "Failed to open partition: %s",
              DI::PrintError(result));
        taskAbort();
    }
}

const Partition*
Apploader::findBootPartition(const Volume& main_volume,
                             const std::array<Partition, 4>& partitions)
{
    for (const auto& part : partitions) {
        PRINT(Loader, INFO, "| Partition: %08X %08X", part.offset, part.type);
    }

    if (main_volume.num_boot_info > partitions.size()) {
        PRINT(Loader, ERROR, "Invalid volume header");
        return nullptr;
    }

    const auto found_it = std::find_if(
        partitions.begin(), partitions.begin() + main_volume.num_boot_info,
        [](const Partition& part) { return part.type == 0; });

    if (found_it == partitions.end()) {
        PRINT(Loader, ERROR, "Couldn't find boot partition");
        return nullptr;
    }

    PRINT(Loader, INFO, "Partition: %08X %08X", found_it->offset,
          found_it->type);
    return &*found_it;
}

std::array<Partition, 4> Apploader::readPartitions(const Volume& volume)
{
    PRINT(Loader, INFO, "Reading partition headers offset: %i..",
          static_cast<s32>(volume.ofs_partition_info));

    std::array<Partition, 4> partitions ATTRIBUTE_ALIGN(32);

    memset(&partitions, 'F', sizeof(partitions));

    UnencryptedRead(partitions.data(), sizeof(partitions),
                    volume.ofs_partition_info);

    return partitions;
}

std::array<Volume, 4> Apploader::readVolumes()
{
    std::array<Volume, 4> volumes ATTRIBUTE_ALIGN(32);
    PRINT(Loader, INFO, "Reading table of contents..");

    memset(&volumes, 0, sizeof(volumes));
    UnencryptedRead(volumes.data(), sizeof(volumes), 0x00010000);
    for (auto& v : volumes) {
        PRINT(Loader, INFO, "| Volume %u %u", v.num_boot_info,
              v.ofs_partition_info);
    }

    return volumes;
}