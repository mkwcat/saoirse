// Based on BrainSlug, by Chadderz

#include "GlobalsConfig.hpp"
#include <ogc/cache.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    char gamename[4];
    char company[2];
    uint8_t disknum;
    uint8_t gamever;
    uint8_t streaming;
    uint8_t streambufsize;
    uint8_t pad[14];
    uint32_t wii_magic;
    uint32_t gc_magic;
} os_disc_id_t;

typedef enum { OS_BOOT_NORMAL = 0x0d15ea5e } os_boot_type_t;

typedef struct {
    uint32_t boot_type;
    uint32_t version;
    uint32_t mem1_size;
    uint32_t console_type;
    uint32_t arena_low;
    uint32_t arena_high;
    void* fst;
    uint32_t fst_size;
} os_system_info_t;

typedef struct {
    uint32_t enabled;
    uint32_t exception_mask;
    void* destination;
    uint8_t temp[0x14];
    uint8_t hook[0x24];
    uint8_t padding[0x3c];
} os_debugger_t;

typedef enum {
    OS_TV_MODE_NTSC,
    OS_TV_MODE_PAL,
    OS_TV_MODE_DEBUG,
    OS_TV_MODE_DEBUG_PAL,
    OS_TV_MODE_MPAL,
    OS_TV_MODE_PAL60,
} os_tv_mode_t;

typedef struct {
    void* current_context_phy;
    uint32_t previous_interrupt_mask;
    uint32_t current_interrupt_mask;
    uint32_t tv_mode;
    uint32_t aram_size;
    void* current_context;
    void* default_thread;
    void* thread_queue_head;
    void* thread_queue_tail;
    void* current_thread;
    uint32_t debug_monitor_size;
    void* debug_monitor_location;
    uint32_t simulated_memory_size;
    void* bi2;
    uint32_t bus_speed;
    uint32_t cpu_speed;
} os_thread_info_t;

typedef struct {
    os_disc_id_t disc;        /* 0x0 */
    os_system_info_t info;    /* 0x20 */
    os_debugger_t debugger;   /* 0x40 */
    os_thread_info_t threads; /* 0xc0 */
} os_early_globals_t;

static os_early_globals_t* const os0 = (os_early_globals_t*)0x80000000;

typedef struct {
    void* exception_handlers[0x10];    /* 0x0 */
    void* irq_handlers[0x20];          /* 0x40 */
    uint8_t paddingc0[0x100 - 0xc0];   /* 0xc0 */
    uint32_t mem1_size;                /* 0x100 */
    uint32_t mem1_simulated_size;      /* 0x104 */
    void* mem1_end;                    /* 0x108 */
    uint8_t padding10c[0x110 - 0x10c]; /* 0x10c */
    void* fst;                         /* 0x110 */
    uint8_t padding114[0x118 - 0x114]; /* 0x114 */
    uint32_t mem2_size;                /* 0x118 */
    uint32_t mem2_simulated_size;      /* 0x11c */
    uint8_t padding120[0x130 - 0x120]; /* 0x120 */
    uint32_t ios_heap_start;           /* 0x130 */
    uint32_t ios_heap_end;             /* 0x134 */
    uint32_t hollywood_version;        /* 0x138 */
    uint8_t padding13c[0x140 - 0x13c]; /* 0x13c */
    uint16_t ios_number;               /* 0x140 */
    uint16_t ios_revision;             /* 0x142 */
    uint32_t ios_build_date;           /* 0x144 */
    uint8_t padding148[0x158 - 0x148]; /* 0x148 */
    uint32_t gddr_vendor_id;           /* 0x158 */
    uint32_t legacy_di;                /* 0x15c */
    uint32_t init_semaphore;           /* 0x160 */
    uint32_t mios_flag;                /* 0x164 */
    uint8_t padding168[0x180 - 0x168]; /* 0x168 */
    char application_name[4];          /* 0x180 */
    os_disc_id_t* id;                  /* 0x184 */
    uint16_t expected_ios_number;      /* 0x188 */
    uint16_t expected_ios_revision;    /* 0x18a */
    uint32_t launch_code;              /* 0x18c */
    uint32_t return_code;              /* 0x190 */
} os_late_globals_t;

static os_late_globals_t* const os1 = (os_late_globals_t*)0x80003000;

uint32_t GetArenaLow() { return os0->info.arena_low; }
void SetArenaLow(uint32_t low) { os0->info.arena_low = low; }
uint32_t GetArenaHigh() { return os0->info.arena_high; }
void SetArenaHigh(uint32_t high) { os0->info.arena_high = high; }

void SetupGlobals(int fst_expand) {
    switch (os0->disc.gamename[3]) {
    case 'E':
    case 'J':
        os0->threads.tv_mode = OS_TV_MODE_NTSC;
        break;
    case 'P':
    case 'D':
    case 'F':
    case 'X':
    case 'Y':
        os0->threads.tv_mode = OS_TV_MODE_PAL;
        break;
    }

    os0->info.boot_type = OS_BOOT_NORMAL;
    os0->info.version = 1;
    os0->info.mem1_size = 0x01800000;

    // [Heap---------------][Mods][FST][END OF MEMORY]
    os0->info.console_type = 1 + ((*(uint32_t*)0xcc00302c) >> 28);
    os0->info.arena_high = os0->info.arena_high - fst_expand;
    os0->info.fst = (char*)os0->info.fst - fst_expand;
    os0->info.fst_size += fst_expand;

    os0->threads.debug_monitor_location = (void*)0x81800000;
    os0->threads.simulated_memory_size = 0x01800000;
    os0->threads.bus_speed = 0x0E7BE2C0;
    os0->threads.cpu_speed = 0x2B73A840;

    os1->ios_number = os1->expected_ios_number;
    os1->ios_revision = os1->expected_ios_revision;

    os1->fst = os0->info.fst;
    memcpy(os1->application_name, os0->disc.gamename, 4);

    DCFlushRange(os0, 0x3f00);
}
