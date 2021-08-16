#include "AppInfo.hpp"
#include "es.h"
#include <gctypes.h>

#include <array>
#include <optional>
#include <span>
#include <stdint.h>

struct Volume {
    u32 num_boot_info;
    u32 ofs_partition_info;
};

struct Partition {
    u32 offset;
    u32 type;
};

class Apploader {
public:
    Apploader() = default;
    ~Apploader() = default;

    static EntryPoint load(int fst_expand = 0);
    static void openBootPartition(ES::TMDFixed<512>* outMeta);

private:
    static void dumpAppInfo(const ApploaderInfo& app_info);

    static ApploaderInfo readAppInfo();

    static void openPartition(const Partition& boot_partition,
        ES::TMDFixed<512>* outMeta);

    static const Partition*
        findBootPartition(const Volume& main_volume,
            const std::array<Partition, 4>& partitions);

    static void prepareDisc();
    static std::array<Volume, 4> readVolumes();

    static std::array<Partition, 4> readPartitions(const Volume& volumes);
};