#include "irse.h"

#include "dvd.h"
#include <sdcard.h>
#include <util.h>
LIBOGC_SUCKS_BEGIN
#include <ogc/machine/processor.h>
#include <wiiuse/wpad.h>
LIBOGC_SUCKS_END

#include <disk.h>
#include <ff.h>

#include <cstring>
#include <mutex>
#include <stdio.h>
#include <unistd.h>

#include "Boot.hpp"
#include "GlobalsConfig.hpp"

#include "IOSBoot.hpp"
#include <saoirse_ios_elf.h>

using namespace irse;

Queue<Stage> irse::events;

static struct {
    void* xfb = NULL;
    GXRModeObj* rmode = NULL;
} display;

static constexpr std::array<const char*, 7> logSources = {
    "Core", "DVD", "Loader", "Payload", "IOS", "FST", "DiskIO"};
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

/*
 * Checks to see if we have any events, like shutdown commands.
 * Returns to the previous stage if none, after waiting for some amount
 * of time.
 */
static Stage stDefault(Stage from)
{
    Stage next;
    if (!events.tryreceive(next)) {
        //! No event
        /* Wait 32 ms */
        usleep(32000);
        return from;
    } else {
        //! Received event
        return next;
    }
}

static Stage stInit([[maybe_unused]] Stage from)
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

    DVD::Init();
    SDCard::Open();
    return Stage::Wait;
}

static Stage stWait([[maybe_unused]] Stage from)
{
    static bool lastDiscState = false;
    static bool lastCardState = false;

    bool discState = DVD::IsInserted();
    bool cardState = SDCard::IsInserted();

    if (discState != lastDiscState) {
        lastDiscState = discState;
        return discState ? Stage::DiscInsert : Stage::DiscEject;
    }

    if (cardState != lastCardState) {
        lastCardState = cardState;
        return cardState ? Stage::SDInsert : Stage::SDEject;
    }

    /* temporary, the UI should handle this */
    WPAD_ScanPads();
    s32 down = WPAD_ButtonsDown(0);
    if (down & WPAD_BUTTON_HOME) {
        return Stage::ReturnToMenu;
    }

    return Stage::Default;
}

static Stage stReturnToMenu([[maybe_unused]] Stage from)
{
    irse::Log(LogS::Core, LogL::WARN, "Exiting...");

    if (DVD::IsInserted()) {
        DVD::ResetDrive(false);
    }
    exit(0);
    /* Should never reach here */
    return Stage::ReturnToMenu;
}

static inline bool startupDrive()
{
    /* If ReadDiskID succeeds here, that means the drive is already started */
    DiErr ret = DVD::ReadDiskID(reinterpret_cast<DVD::DiskID*>(MEM1_BASE));
    if (ret == DiErr::OK) {
        irse::Log(LogS::Core, LogL::INFO, "Drive is already spinning");
        return true;
    }
    if (ret != DiErr::DriveError)
        return false;

    /* Drive is not spinning */
    irse::Log(LogS::Core, LogL::INFO, "Spinning up drive...");
    ret = DVD::ResetDrive(true);
    if (ret != DiErr::OK)
        return false;

    ret = DVD::ReadDiskID(reinterpret_cast<DVD::DiskID*>(MEM1_BASE));
    return ret == DiErr::OK;
}

static Stage stDiscInsert([[maybe_unused]] Stage from)
{
    if (!startupDrive())
        return Stage::DiscError;

    DiErr ret = DVD::ReadDiskID(reinterpret_cast<DVD::DiskID*>(MEM1_BASE));
    if (ret != DiErr::OK) {
        irse::Log(LogS::Core, LogL::ERROR, "DVD::ReadDiskID returned %s",
                  DVDLow::PrintErr(ret));
        return Stage::DiscError;
    }

    return Stage::ReadDisc;
}

static Stage stDiscEject([[maybe_unused]] Stage from)
{
    irse::Log(LogS::Core, LogL::INFO, "Disc ejected");
    /* Notify UI controller of eject here */
    return Stage::Wait;
}

static Stage stDiscError([[maybe_unused]] Stage from)
{
    irse::Log(LogS::Core, LogL::ERROR, "Disc error! Waiting for eject...");
    /* Notify UI controller of error here */
    return Stage::Wait;
}

#include "fst.h"

#if 0
static void fstTest()
{
    FSTEntry* fst = *reinterpret_cast<FSTEntry**>(0x80000038);
    FSTBuilder::DirEntry* root;
    {
        FSTReader reader;
        root = reader.process(fst, 0x81800000 - reinterpret_cast<u32>(fst));
        assert(root != nullptr);
    }

    FSTBuilder::Entry* castle_course = root->find("Race/Course/castle_course.szs");
    FSTBuilder::Entry* senior_course = root->find("Race/Course/rainbow_course.szs");

    assert(castle_course != nullptr);
    assert(senior_course != nullptr);

    castle_course->file()->m_wordOffset = senior_course->file()->m_wordOffset;
    castle_course->file()->m_byteLength = senior_course->file()->m_byteLength;

    {
        FSTBuilder builder;
        builder.build(root);
        builder.write(reinterpret_cast<void*>(fst));
    }

    irse::Log(LogS::Core, LogL::WARN,
        "Overwrote castle_course with rainbow_course");
}
#endif

static DIP::DVDPatch fstTest()
{
    assert(FSServ::MountSDCard());
    FIL fil;
    FRESULT fret = f_open(&fil, "0:/beginner_course.szs", FA_READ);
    printf("f_open result: %d\n", fret);
    if (fret != FR_OK) {
        sleep(2);
        abort();
    }

    DIP::DVDPatch patch = {.disc_offset = 0x80000000,
                           .disc_length = static_cast<u32>(f_size(&fil)),
                           .start_cluster = fil.obj.sclust,
                           .cur_cluster = fil.clust,
                           .file_offset = 0,
                           .drv = 0};

    assert(FSServ::UnmountSDCard());

    FSTEntry* fst = *reinterpret_cast<FSTEntry**>(0x80000038);
    FSTBuilder::DirEntry* root;
    {
        FSTReader reader;
        root = reader.process(fst, 0x81800000 - reinterpret_cast<u32>(fst));
        assert(root != nullptr);
    }

    FSTBuilder::Entry* dvdFile = root->find("Race/Course/beginner_course.szs");
    assert(dvdFile != nullptr);

    dvdFile->file()->m_wordOffset = patch.disc_offset;
    dvdFile->file()->m_byteLength = patch.disc_length;

    /* remove entry test */
    auto ent = root->find("thp");
    root->remove(ent);
    delete ent;

    {
        FSTBuilder builder;
        builder.build(root);
        builder.write(reinterpret_cast<void*>(fst));
    }

    return patch;
}

static Stage stReadDisc([[maybe_unused]] Stage from)
{
    irse::Log(LogS::Core, LogL::INFO, "DiskID: %.6s",
              reinterpret_cast<char*>(MEM1_BASE));

    static Apploader loader;
    ES::TMDFixed<512> meta ATTRIBUTE_ALIGN(32);

    loader.openBootPartition(&meta);
    auto main = loader.load();

    DVD::Deinit();
    WPAD_Shutdown();

    DIP::DVDPatch patch = fstTest();

    sleep(1);

    /* Cast as s32 removes high word the in title ID */
    irse::Log(LogS::Core, LogL::INFO, "Launching IOS%d",
              static_cast<s32>(meta.sysVersion));
    IOS_ReloadIOS(static_cast<s32>(meta.sysVersion));

    irse::Log(LogS::Core, LogL::INFO, "Starting up IOS...");
    const s32 ret = IOSBoot::Launch(saoirse_ios_elf, saoirse_ios_elf_size);
    irse::Log(LogS::Core, LogL::INFO, "IOS Launch result: %d", ret);

    IOSBoot::Log* log = new IOSBoot::Log();
    usleep(64000);

    sleep(1);
    DVD::Init();
    startupDrive();

    loader.openBootPartition(&meta);

    DVDProxy::ApplyPatches(&patch, 1);
    // DVDProxy::StartGame();

    DVD::Deinit();
    sleep(1);

    delete log;

    SetupGlobals(0);
    //patchMkwDIPath();

    // TODO: Proper shutdown
    SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
    IRQ_Disable();
    main();
    /* Unreachable! */
    abort();
}

static Stage stSDInsert([[maybe_unused]] Stage from)
{
    irse::Log(LogS::Core, LogL::INFO, "SD Card inserted");
    return Stage::Wait;
}

static Stage stSDEject([[maybe_unused]] Stage from)
{
    irse::Log(LogS::Core, LogL::INFO, "SD Card ejected");
    return Stage::Wait;
}

static s32 Loop([[maybe_unused]] void* arg)
{
    Stage stage = Stage::Init;
    Stage prev = Stage::Default;
    Stage next = Stage::ReturnToMenu;

    while (1) {
        switch (stage) {
#define STAGE_CASE(name)                                                       \
    case Stage::name:                                                          \
        next = st##name(prev);                                                 \
        break
            STAGE_CASE(Default);
            STAGE_CASE(Init);
            STAGE_CASE(Wait);
            STAGE_CASE(ReturnToMenu);
            STAGE_CASE(DiscInsert);
            STAGE_CASE(DiscEject);
            STAGE_CASE(DiscError);
            STAGE_CASE(ReadDisc);
            STAGE_CASE(SDInsert);
            STAGE_CASE(SDEject);
#undef STAGE_CASE
        }
        prev = stage;
        stage = next;
    }
}

s32 main([[maybe_unused]] s32 argc, [[maybe_unused]] char** argv)
{
    if (IOS_GetVersion() != 58)
        IOS_ReloadIOS(58);
    return Loop(0);
}
