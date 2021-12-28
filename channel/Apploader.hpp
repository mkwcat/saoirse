#include "AppInfo.hpp"
#include "AppPayload.hpp"
#include <es.h>
#include <gctypes.h>

#include <array>
#include <future>
#include <optional>
#include <span>
#include <stdint.h>
#include <util.h>

struct Volume {
    u32 num_boot_info;
    u32 ofs_partition_info;
};

struct Partition {
    u32 offset;
    u32 type;
};

class Apploader
{
public:
    Apploader() = default;
    ~Apploader() = default;

    static s32 threadEntry(void* arg);

    EntryPoint load(int fst_expand = 0);

    enum class TickResult
    {
        Continue,
        Error,
        Done
    };
    TickResult tick();

    void openBootPartition(ES::TMDFixed<512>* outMeta);

private:
    void dumpAppInfo(const ApploaderInfo& app_info);

    ApploaderInfo readAppInfo();

    void openPartition(const Partition& boot_partition,
                       ES::TMDFixed<512>* outMeta);

    const Partition*
    findBootPartition(const Volume& main_volume,
                      const std::array<Partition, 4>& partitions);

    void prepareDisc();
    std::array<Volume, 4> readVolumes();

    std::array<Partition, 4> readPartitions(const Volume& volumes);

    enum class TickStage
    {
        ReadVolumes,
        ReadPartitions,
        OpenPartition,
        LoadApploader,
        LoadSegment,
        GetEntryPoint
    };
    TickStage m_stage;

    Volume m_mainVolume;
    std::array<Partition, 4> m_partitions;
    const Partition* m_bootPartition;
    ES::TMDFixed<512> m_meta ATTRIBUTE_ALIGN(32);

    AppPayload m_payload;
    EntryPoint m_entryPoint;
};