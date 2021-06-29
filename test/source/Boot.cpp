#include "AppPayload.hpp"
#include "Boot.hpp"
#include "dvd.h"
#include "util.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <gcutil.h>
#include <iostream>
#include <ogc/cache.h>
#include <ogc/es.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
#include <optional>
#include <span>
#include <stdint.h>
#include <string.h>
#include <thread>
#include <time.h>

#if 0
static void DebugPause() {
    using namespace std;
    std::this_thread::sleep_for(2s);
}
#else
static void DebugPause() {}
#endif

enum class CachePolicy {
    None,
    InvalidateDC,
    FlushDC
};

static void EnforceCachePolicy(void* dst, u32 len, CachePolicy policy) {
    if (policy == CachePolicy::InvalidateDC) {
        DCInvalidateRange(dst, len);
    }
    if (policy == CachePolicy::FlushDC) {
        DCFlushRange(dst, len);
    }
}

static void UnencryptedRead(void* dst, u32 len, u32 ofs, CachePolicy invalidate) {
    DVD::UniqueCommand cmd;
    assert(cmd.cmd() != nullptr);
    DVDLow::UnencryptedReadAsync(*cmd.cmd(), dst, len, ofs);
    const auto result = cmd.cmd()->syncReply();

    if (result != DiErr::OK) {
        printf("Failed to execute read: RESULT=%i\n", static_cast<s32>(result));
        return;
    }

    EnforceCachePolicy(dst, len, invalidate);
}

static void EncryptedRead(void* dst, u32 len, u32 ofs, CachePolicy invalidate) {
    DVD::UniqueCommand cmd;
    assert(cmd.cmd() != nullptr);
    DVDLow::EncryptedReadAsync(*cmd.cmd(), dst, len, ofs);
    const auto result = cmd.cmd()->syncReply();

    if (result != DiErr::OK) {
        printf("Failed to execute encrypted read\n");
        return;
    }

    EnforceCachePolicy(dst, len, invalidate);
}


void OpenPartition(s32 ofs, void* tmd) {
    DVD::UniqueCommand cmd;
    assert(cmd.cmd() != nullptr);
    DVDLow::OpenPartitionAsync(*cmd.cmd(), ofs, reinterpret_cast<signed_blob*>(tmd));
    const auto result = cmd.cmd()->syncReply();

    if (result != DiErr::OK) {
        printf("Failed to open partition\n");
        abort();
    }
}

class PayloadManager {
public:
    PayloadManager(const ApploaderInfo& info) : mPayload(info) {}

    void loadSegmnts() {
        while (true) {
            const std::optional<AppPayload::CopyCommand> copy_cmd =
                mPayload.popCopyCommand();
            if (!copy_cmd.has_value())
                break;

            EncryptedRead(copy_cmd->dest, copy_cmd->length, round_down(copy_cmd->offset, 4), CachePolicy::FlushDC);
        }
    }

    EntryPoint getEntrypoint() const { return mPayload.get_entrypoint(); }

private:
    AppPayload mPayload;
};

EntryPoint Apploader::load(
    // TODO: If we want to shrink the FST to fit some code a-la CTGP.
    [[maybe_unused]] int fst_expand)
{
    const Volume main_volume = readVolumes()[0];
    
    const auto partitions = readPartitions(main_volume);
    DebugPause();
    const Partition* boot_partition = findBootPartition(main_volume, partitions);
    DebugPause();
    if (boot_partition == nullptr) {
        return nullptr;
    }
    printf("Boot partition: %p\n", reinterpret_cast<const void*>(boot_partition));

    openPartition(*boot_partition);
    DebugPause();
    const ApploaderInfo app_info = readAppInfo();
    dumpAppInfo(app_info);
    DebugPause();

    
    // XXX Partition must be open
    PayloadManager payload(app_info);
    DebugPause();
    settime(secs_to_ticks(time(NULL) - 946684800));
    DebugPause();
    payload.loadSegmnts();

    DebugPause();


    return payload.getEntrypoint();
}

void Apploader::dumpAppInfo(const ApploaderInfo& app_info) {
    auto mem_dump = [](std::span<u32> mem) {
        printf("MEMORY DUMP\n");
        for (std::size_t i = 0; i < mem.size(); i += 4) {
            printf("%p: %08X %08X %08X %08X\n",
                reinterpret_cast<void*>(&mem[i]),
                mem[i], mem[i + 1],
                mem[i + 2], mem[i + 3]);
        }
    };
    mem_dump({ (u32*)&app_info, 32 });
}

ApploaderInfo Apploader::readAppInfo() {
    static ApploaderInfo app_info ATTRIBUTE_ALIGN(32);

    printf("Reading apploader info..\n");
    EncryptedRead(&app_info, sizeof(app_info), 0x2440 / 4, CachePolicy::InvalidateDC);
    DebugPause();
    return app_info;
}

void Apploader::openPartition(const Partition& boot_partition) {
    printf("Reading boot partition..\n");
    std::array<u32, 0x4A00 / 4> ticket_metadata ATTRIBUTE_ALIGN(32) = {};
    OpenPartition(boot_partition.offset, ticket_metadata.data());

    printf("...\n");
}

const Partition*
Apploader::findBootPartition(const Volume& main_volume,
    const std::array<Partition, 4>& partitions) {

    for (const auto& part : partitions) {
        printf("| Partition: %08x %08x\n", part.offset, part.type);
    }

    if (main_volume.num_boot_info > partitions.size()) {
        printf("Invalid volume header.\n");
        return nullptr;
    }

    const auto found_it = std::find_if(
        partitions.begin(), partitions.begin() + main_volume.num_boot_info,
        [](const Partition& part) { return part.type == 0; });

    if (found_it == partitions.end()) {
        printf("Couldn't find boot partition.\n");
        return nullptr;
    }

    printf("Partition: %08x %08x\n", found_it->offset, found_it->type);
    return &*found_it;
}

std::array<Partition, 4> Apploader::readPartitions(const Volume& volume) {
    printf("Reading partition headers offset: %i..\n", static_cast<s32>(volume.ofs_partition_info));

    std::array<Partition, 4> partitions ATTRIBUTE_ALIGN(32);

    memset(&partitions, 'F', sizeof(partitions));

    UnencryptedRead(partitions.data(), sizeof(partitions), volume.ofs_partition_info, CachePolicy::InvalidateDC);
    
    return partitions;
}

std::array<Volume, 4> Apploader::readVolumes() {
    std::array<Volume, 4> volumes ATTRIBUTE_ALIGN(32);
    printf("Reading table of contents..\n");

    memset(&volumes, 0, sizeof(volumes));
    UnencryptedRead(volumes.data(), sizeof(volumes), 0x00010000, CachePolicy::InvalidateDC);
    for (auto& v : volumes) {
        printf("| Volume %u %u\n", v.num_boot_info, v.ofs_partition_info);
    }

    return volumes;
}