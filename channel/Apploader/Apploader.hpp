#include "AppInfo.hpp"
#include "AppPayload.hpp"
#include "TaskThread.hpp"
#include <es.h>
#include <gctypes.h>

#include <array>
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

class Apploader : public TaskThread
{
public:
    Apploader(EntryPoint* result)
    {
        m_resultOut = result;
    }
    ~Apploader() = default;

protected:
    void taskEntry();

private:
    EntryPoint load(int fst_expand = 0);

    void openBootPartition(ES::TMDFixed<512>* outMeta);

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

    ES::TMDFixed<512> m_meta ATTRIBUTE_ALIGN(32);
    EntryPoint* m_resultOut;
};