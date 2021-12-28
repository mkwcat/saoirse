#include "Apploader.hpp"
#include "AppPayload.hpp"
#include "dvd.h"
#include "irse.h"
#include <util.h>
#include <sdcard.h>
#include "GlobalsConfig.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <gcutil.h>
#include <iostream>
LIBOGC_SUCKS_BEGIN
#include <ogc/cache.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
#include <ogc/video.h>
#include <ogc/irq.h>
LIBOGC_SUCKS_END
#include <optional>
#include <span>
#include <stdint.h>
#include <string.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include "IOSBoot.hpp"

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

s32 Apploader::threadEntry([[maybe_unused]] void* arg)
{
    // TODO use abstract DVD interface for different types of discs (like
    // backups etc)
    DVD::Init();
    if (!startupDrive()) {
        irse::Log(LogS::Loader, LogL::ERROR, "Drive spinup failed");
        abort();
    }

    Apploader apploader;

    while (1) {
        Apploader::TickResult result = apploader.tick();

        if (result == Apploader::TickResult::Continue)
            continue;
        if (result == Apploader::TickResult::Done)
            break;

        irse::Log(LogS::Loader, LogL::ERROR, "Apploader tick returned error");
        abort();
    }
    SDCard::Shutdown();
    DVD::Deinit();

    IOSBoot::Log::sInstance->startGameIOS();
    auto entry = apploader.m_entryPoint;

    delete IOSBoot::Log::sInstance;

    VIDEO_SetBlack(true);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    SetupGlobals(0);
    // patchMkwDIPath();

    // TODO: Proper shutdown
    SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
    IRQ_Disable();

    entry();
    /* Unreachable! */
    abort();

    return 0;
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

enum class CachePolicy
{
    None,
    InvalidateDC,
    FlushDC
};

static void EnforceCachePolicy(void* dst, u32 len, CachePolicy policy)
{
    if (policy == CachePolicy::InvalidateDC) {
        DCInvalidateRange(dst, len);
    }
    if (policy == CachePolicy::FlushDC) {
        DCFlushRange(dst, len);
    }
}

static void UnencryptedRead(void* dst, u32 len, u32 ofs, CachePolicy invalidate)
{
    DVD::UniqueCommand cmd;
    assert(cmd.cmd() != nullptr);
    DVDLow::UnencryptedReadAsync(*cmd.cmd(), dst, len, ofs);
    const auto result = cmd.cmd()->syncReply();

    if (result != DiErr::OK) {
        irse::Log(LogS::Loader, LogL::ERROR, "Failed to execute read: %s",
                  DVDLow::PrintErr(result));
        return;
    }

    EnforceCachePolicy(dst, len, invalidate);
}

static void EncryptedRead(void* dst, u32 len, u32 ofs, CachePolicy invalidate)
{
    DVD::UniqueCommand cmd;
    assert(cmd.cmd() != nullptr);
    DVDLow::EncryptedReadAsync(*cmd.cmd(), dst, len, ofs);
    const auto result = cmd.cmd()->syncReply();

    if (result != DiErr::OK) {
        irse::Log(LogS::Loader, LogL::ERROR,
                  "Failed to execute encrypted read");
        return;
    }

    EnforceCachePolicy(dst, len, invalidate);
}

void OpenPartition(s32 ofs, ES::TMDFixed<512>* meta)
{
    DVD::UniqueCommand cmd;
    assert(cmd.cmd() != nullptr);
    DVDLow::OpenPartitionAsync(*cmd.cmd(), ofs, meta);
    const auto result = cmd.cmd()->syncReply();

    if (result != DiErr::OK) {
        irse::Log(LogS::Loader, LogL::ERROR, "Failed to open partition: %s",
                  DVDLow::PrintErr(result));
        abort();
    }
}

#if 0
EntryPoint Apploader::load(
    // TODO: If we want to shrink the FST to fit some code a-la CTGP.
    [[maybe_unused]] int fst_expand)
{
    const ApploaderInfo app_info = readAppInfo();
    dumpAppInfo(app_info);
    DebugPause();

    // XXX Partition must be open
    PayloadManager payload(app_info);
    DebugPause();
    settime(secs_to_ticks(time(NULL) - 946684800));
    DebugPause();
    payload.loadAllSegments();

    DebugPause();
    return payload.getEntrypoint();
}
#endif

Apploader::TickResult Apploader::tick()
{
    switch (m_stage) {
    case TickStage::ReadVolumes:
        m_mainVolume = readVolumes()[0];
        m_stage = TickStage::ReadPartitions;
        break;

    case TickStage::ReadPartitions:
        m_partitions = readPartitions(m_mainVolume);
        m_stage = TickStage::OpenPartition;
        break;

    case TickStage::OpenPartition:
        m_bootPartition = findBootPartition(m_mainVolume, m_partitions);
        if (m_bootPartition == nullptr) {
            irse::Log(LogS::Loader, LogL::ERROR,
                      "Failed to find boot partition");
            return TickResult::Error;
        }
        openPartition(*m_bootPartition, &m_meta);
        m_stage = TickStage::LoadApploader;
        break;

    case TickStage::LoadApploader: {
        const ApploaderInfo appInfo = readAppInfo();
        dumpAppInfo(appInfo);
        m_payload.init(appInfo);
        m_stage = TickStage::LoadSegment;
        break;
    }

    case TickStage::LoadSegment: {
        const std::optional<AppPayload::CopyCommand> copy_cmd =
            m_payload.popCopyCommand();
        if (!copy_cmd.has_value()) {
            m_stage = TickStage::GetEntryPoint;
            break;
        }

        EncryptedRead(copy_cmd->dest, copy_cmd->length,
                      round_down(copy_cmd->offset, 4), CachePolicy::None);
        break;
    }

    case TickStage::GetEntryPoint:
        m_entryPoint = m_payload.get_entrypoint();
        return TickResult::Done;
    }

    return TickResult::Continue;
}

void Apploader::openBootPartition(ES::TMDFixed<512>* outMeta)
{
    const Volume main_volume = readVolumes()[0];

    const auto partitions = readPartitions(main_volume);
    DebugPause();
    const Partition* boot_partition =
        findBootPartition(main_volume, partitions);
    DebugPause();
    if (boot_partition == nullptr) {
        irse::Log(LogS::Loader, LogL::ERROR, "Failed to find boot partition");
        abort();
    }
    irse::Log(LogS::Loader, LogL::INFO, "Boot partition: %p",
              reinterpret_cast<const void*>(boot_partition));

    openPartition(*boot_partition, outMeta);
}

void Apploader::dumpAppInfo(const ApploaderInfo& app_info)
{
    auto mem_dump = [](std::span<u32> mem) {
        irse::Log(LogS::Loader, LogL::INFO, "MEMORY DUMP");
        for (std::size_t i = 0; i < mem.size(); i += 4) {
            irse::Log(LogS::Loader, LogL::INFO, "%p: %08X %08X %08X %08X",
                      reinterpret_cast<void*>(&mem[i]), mem[i], mem[i + 1],
                      mem[i + 2], mem[i + 3]);
        }
    };
    mem_dump({(u32*)&app_info, 32});
}

ApploaderInfo Apploader::readAppInfo()
{
    static ApploaderInfo app_info ATTRIBUTE_ALIGN(32);

    irse::Log(LogS::Loader, LogL::INFO, "Reading apploader info..");
    EncryptedRead(&app_info, sizeof(app_info), 0x2440 / 4, CachePolicy::None);
    DebugPause();
    return app_info;
}

void Apploader::openPartition(const Partition& boot_partition,
                              ES::TMDFixed<512>* outMeta)
{
    irse::Log(LogS::Loader, LogL::INFO, "Reading boot partition..");
    OpenPartition(boot_partition.offset, outMeta);
}

const Partition*
Apploader::findBootPartition(const Volume& main_volume,
                             const std::array<Partition, 4>& partitions)
{
    for (const auto& part : partitions) {
        irse::Log(LogS::Loader, LogL::INFO, "| Partition: %08X %08X",
                  part.offset, part.type);
    }

    if (main_volume.num_boot_info > partitions.size()) {
        irse::Log(LogS::Loader, LogL::ERROR, "Invalid volume header");
        return nullptr;
    }

    const auto found_it = std::find_if(
        partitions.begin(), partitions.begin() + main_volume.num_boot_info,
        [](const Partition& part) { return part.type == 0; });

    if (found_it == partitions.end()) {
        irse::Log(LogS::Loader, LogL::ERROR, "Couldn't find boot partition");
        return nullptr;
    }

    irse::Log(LogS::Loader, LogL::INFO, "Partition: %08X %08X",
              found_it->offset, found_it->type);
    return &*found_it;
}

std::array<Partition, 4> Apploader::readPartitions(const Volume& volume)
{
    irse::Log(LogS::Loader, LogL::INFO,
              "Reading partition headers offset: %i..",
              static_cast<s32>(volume.ofs_partition_info));

    std::array<Partition, 4> partitions ATTRIBUTE_ALIGN(32);

    memset(&partitions, 'F', sizeof(partitions));

    UnencryptedRead(partitions.data(), sizeof(partitions),
                    volume.ofs_partition_info, CachePolicy::None);

    return partitions;
}

std::array<Volume, 4> Apploader::readVolumes()
{
    std::array<Volume, 4> volumes ATTRIBUTE_ALIGN(32);
    irse::Log(LogS::Loader, LogL::INFO, "Reading table of contents..");

    memset(&volumes, 0, sizeof(volumes));
    UnencryptedRead(volumes.data(), sizeof(volumes), 0x00010000,
                    CachePolicy::None);
    for (auto& v : volumes) {
        irse::Log(LogS::Loader, LogL::INFO, "| Volume %u %u", v.num_boot_info,
                  v.ofs_partition_info);
    }

    return volumes;
}