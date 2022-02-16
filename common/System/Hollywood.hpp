// Hollywood.hpp - Wii Hardware I/O
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once
#include <System/Types.h>
#include <System/Util.h>

// PPC base for ACR
constexpr u32 HW_BASE = 0x0D000000;
// IOP base for ACR
constexpr u32 HW_BASE_TRUSTED = 0x0D800000;

// The 0x00800000 bit gets masked out when PPC tries to access the trusted base.

// ACR (Hollywood Registers)
enum class ACRReg
{
    IPC_PPCMSG = 0x000,
    IPC_PPCCTRL = 0x004,
    IPC_ARMMSG = 0x008,
    IPC_ARMCTRL = 0x00C,

    VISOLID = 0x024,

    PPC_IRQFLAG = 0x030,
    PPC_IRQMASK = 0x034,
    ARM_IRQFLAG = 0x038,
    ARM_IRQMASK = 0x03C,

    // Controls access to the IOP SRAM
    SRNPROT = 0x060,
    // Bus access for the PPC and TRUSTED bases
    BUSPROT = 0x064,

    // Restricted Broadway GPIO access
    GPIOB_OUT = 0x0C0,
    GPIOB_DIR = 0x0C4,
    GPIOB_IN = 0x0C8,
    // Full GPIO access
    GPIO_OUT = 0x0E0,
    GPIO_DIR = 0x0E4,
    GPIO_IN = 0x0E8,

    RESETS = 0x194,
};

// Bit fields for SRNPROT
enum class ACRSRNPROTBit
{
    // Enables the AES engine access to SRAM
    AESEN = 0x01,
    // Enables the SHA-1 engine access to SRAM
    SHAEN = 0x02,
    // Enables the Flash/NAND engine access to SRAM
    FLAEN = 0x04,
    // Enables PPC access to SRAM; Set/cleared by syscall (0x54)
    AHPEN = 0x08,
    // Enables OH1 access to SRAM
    OH1EN = 0x10,
    // Enables the SRAM mirror at 0xFFFE0000
    IOUEN = 0x20,
    IOPDBGEN = 0x40,
};

// Bit fields for BUSPROT
enum class ACRBUSPROTBit : u32
{
    // Flash/NAND Engine PPC; Set/cleared by syscall_54
    PPCFLAEN = 0x00000002,
    // AES Engine PPC; Set/cleared by syscall_54
    PPCAESEN = 0x00000004,
    // SHA-1 Engine PPC; Set/cleared by syscall_54
    PPCSHAEN = 0x00000008,
    // Enhanced Host Interface PPC; Set/cleared by syscall_54
    PPCEHCEN = 0x00000010,
    // Open Host Interface #0 PPC; Set/cleared by syscall_54
    PPC0H0EN = 0x00000020,
    // Open Host Interface #1 PPC; Set/cleared by syscall_54
    PPC0H1EN = 0x00000040,
    // SD Interface #0 PPC; Set/cleared by syscall_54
    PPCSD0EN = 0x00000080,
    // SD Interface #1 PPC; Set/cleared by syscall_54
    PPCSD1EN = 0x00000100,
    // ?? Set/cleared by syscall_54
    PPCSREN = 0x00000400,
    // ?? Set/cleared by syscall_54
    PPCAHMEN = 0x00000800,

    // Flash/NAND Engine IOP
    IOPFLAEN = 0x00020000,
    // AES Engine IOP
    IOPAESEN = 0x00040000,
    // SHA-1 Engine IOP
    IOPSHAEN = 0x00080000,
    // Enhanced Host Interface IOP
    IOPEHCEN = 0x00100000,
    // Open Host Interface #0 IOP
    IOP0H0EN = 0x00200000,
    // Open Host Interface #1 IOP
    IOP0H1EN = 0x00400000,
    // SD Interface #0 IOP
    IOPSD0EN = 0x00800000,
    // SD Interface #1 IOP
    IOPSD1EN = 0x01000000,

    // Gives PPC full read & write access to ACR that is normally only
    // accessible to IOP; Set/cleared by syscall_54
    PPCKERN = 0x80000000,
};

// GPIO pin connections
enum class GPIOPin
{
    POWER = 0x000001,
    SHUTDOWN = 0x000002,
    FAN = 0x000004,
    DC_DC = 0x000008,
    DI_SPIN = 0x000010,
    SLOT_LED = 0x000020,
    EJECT_BTN = 0x000040,
    SLOT_IN = 0x000080,
    // Sensor Bar on/off
    SENSOR_BAR = 0x000100,
    DO_EJECT = 0x000200,
    EEP_CS = 0x000400,
    EEP_CLK = 0x000800,
    EEP_MOSI = 0x001000,
    EEP_MISO = 0x002000,
    AVE_SCL = 0x004000,
    AVE_SDA = 0x008000,
    DEBUG0 = 0x010000,
    DEBUG1 = 0x020000,
    DEBUG2 = 0x040000,
    DEBUG3 = 0x080000,
    DEBUG4 = 0x100000,
    DEBUG5 = 0x200000,
    DEBUG6 = 0x400000,
    DEBUG7 = 0x800000,
};

// HW_RESETS flags
enum class ACRResetLine
{
    // System reset
    RSTBINB = 0x0000001,
    // CRST reset?
    CRSTB = 0x0000002,
    // RSTB reset?
    RSTB = 0x0000004,
    // DSKPLL reset
    RSTB_DSKPLL = 0x0000008,
    // PowerPC HRESET
    RSTB_CPU = 0x0000010,
    // PowerPC SRESET
    SRSTB_CPU = 0x0000020,
    // SYSPLL reset
    RSTB_SYSPLL = 0x0000040,
    // Unlock SYSPLL reset?
    NLCKB_SYSPLL = 0x0000080,
    // MEM reset B
    RSTB_MEMRSTB = 0x0000100,
    // PI reset
    RSTB_PI = 0x0000200,
    // Drive Interface reset B
    RSTB_DIRSTB = 0x0000400,
    // MEM reset
    RSTB_MEM = 0x0000800,
    // GFX TCPE?
    RSTB_GFXTCPE = 0x0001000,
    // GFX reset?
    RSTB_GFX = 0x0002000,
    // Audio Interface I2S3 reset
    RSTB_AI_I2S3 = 0x0004000,
    // Serial Interface I/O reset
    RSTB_IOSI = 0x0008000,
    // External Interface I/O reset
    RSTB_IOEXI = 0x0010000,
    // Drive Interface I/O reset
    RSTB_IODI = 0x0020000,
    // MEM I/O reset
    RSTB_IOMEM = 0x0040000,
    // Processor Interface I/O reset
    RSTB_IOPI = 0x0080000,
    // Video Interface reset
    RSTB_VI = 0x0100000,
    // VI1 reset?
    RSTB_VI1 = 0x0200000,
    // IOP reset
    RSTB_IOP = 0x0400000,
    // ARM AHB reset
    RSTB_AHB = 0x0800000,
    // External DRAM reset
    RSTB_EDRAM = 0x1000000,
    // Unlock external DRAM reset?
    NLCKB_EDRAM = 0x2000000,
};

// Read ACR register.
// Requires BUSPROT::PPCKERN set if used from Broadway.
static inline u32 ACRReadTrusted(ACRReg reg)
{
    return read32(HW_BASE_TRUSTED + static_cast<u32>(reg));
}

// Read ACR register in the untrusted Broadway mirror.
static inline u32 ACRRead(ACRReg reg)
{
    return read32(HW_BASE + static_cast<u32>(reg));
}

// Write ACR register.
// Requires BUSPROT::PPCKERN set if used from Broadway.
static inline void ACRWriteTrusted(ACRReg reg, u32 value)
{
    write32(HW_BASE_TRUSTED + static_cast<u32>(reg), value);
}

// Write ACR register in the untrusted Broadway mirror.
static inline void ACRWrite(ACRReg reg, u32 value)
{
    write32(HW_BASE + static_cast<u32>(reg), value);
}

// Mask ACR register.
// Requires BUSPROT::PPCKERN set if used from Broadway.
static inline void ACRMaskTrusted(ACRReg reg, u32 clear, u32 set)
{
    mask32(HW_BASE_TRUSTED + static_cast<u32>(reg), clear, set);
}

//  Mask ACR register in the untrusted Broadway mirror.
static inline void ACRMask(ACRReg reg, u32 clear, u32 set)
{
    mask32(HW_BASE + static_cast<u32>(reg), clear, set);
}

// Read from a GPIO pin owned by Broadway.
static inline bool GPIOBRead(GPIOPin pin)
{
    return static_cast<bool>(ACRRead(ACRReg::GPIOB_IN) & static_cast<u32>(pin));
}

// Write to a GPIO pin owned by Broadway.
static inline void GPIOBWrite(GPIOPin pin, bool flag)
{
    ACRMask(ACRReg::GPIOB_OUT, static_cast<u32>(pin),
            flag ? static_cast<u32>(pin) : 0);
}

// Read from a GPIO pin owned by IOP.
static inline bool GPIORead(GPIOPin pin)
{
    return static_cast<bool>(ACRReadTrusted(ACRReg::GPIO_IN) &
                             static_cast<u32>(pin));
}

// Write to a GPIO pin owned by IOP.
static inline void GPIOWrite(GPIOPin pin, bool flag)
{
    ACRMaskTrusted(ACRReg::GPIO_OUT, static_cast<u32>(pin),
                   flag ? static_cast<u32>(pin) : 0);
}

// Assert or deassert a reset line.
// flag: false = assert/off, true = deassert/on.
static inline void ACRReset(ACRResetLine line, bool flag)
{
    ACRMaskTrusted(ACRReg::RESETS, static_cast<u32>(line),
                   flag ? static_cast<u32>(line) : 0);
}

// Get the status of a reset line.
// return value: false = assert/off, true = deassert/on.
static inline bool ACRCheckReset(ACRResetLine line)
{
    return (ACRReadTrusted(ACRReg::RESETS) & static_cast<u32>(line)) != 0;
}

// Set a flag in an ACR register. Register is determined using bit type.
// flag: false = off, true = on.
static inline void ACRSetFlag(ACRSRNPROTBit bit, bool flag)
{
    ACRMaskTrusted(ACRReg::SRNPROT, static_cast<u32>(bit),
                   flag ? static_cast<u32>(bit) : 0);
}

// Get the state of an ACR register flag. Register is determined using bit type.
// return value: false = off, true = on.
static inline bool ACRReadFlag(ACRSRNPROTBit bit)
{
    return (ACRReadTrusted(ACRReg::SRNPROT) & static_cast<u32>(bit)) != 0;
}

// Set a flag in an ACR register. Register is determined using bit type.
// flag: false = off, true = on.
static inline void ACRSetFlag(ACRBUSPROTBit bit, bool flag)
{
    ACRMaskTrusted(ACRReg::BUSPROT, static_cast<u32>(bit),
                   flag ? static_cast<u32>(bit) : 0);
}

// Get the state of an ACR register flag. Register is determined using bit type.
// return value: false = off, true = on.
static inline bool ACRReadFlag(ACRBUSPROTBit bit)
{
    return (ACRReadTrusted(ACRReg::BUSPROT) & static_cast<u32>(bit)) != 0;
}

// Memory Controller Registers
enum class MEMCRReg
{
    // DDR protection enable/disable
    MEM_PROT_DDR = 0xB420A,
    // DDR protection base address
    MEM_PROT_DDR_BASE = 0xB420C,
    // DDR protection end address
    MEM_PROT_DDR_END = 0xB420E,
};

// Read MEMCR register.
// Requires BUSPROT::PPCKERN set if used from Broadway.
static inline u16 MEMCRReadTrusted(MEMCRReg reg)
{
    return read16(HW_BASE_TRUSTED + static_cast<u32>(reg));
}

// Read MEMCR register in the untrusted Broadway mirror.
static inline u16 MEMCRRead(MEMCRReg reg)
{
    return read16(HW_BASE + static_cast<u32>(reg));
}

// Write MEMCR register.
// Requires BUSPROT::PPCKERN set if used from Broadway.
static inline void MEMCRWriteTrusted(MEMCRReg reg, u16 value)
{
    write16(HW_BASE_TRUSTED + static_cast<u32>(reg), value);
}

// Write MEMCR register in the untrusted Broadway mirror.
static inline void MEMCRWrite(MEMCRReg reg, u16 value)
{
    write16(HW_BASE + static_cast<u32>(reg), value);
}

// Mask MEMCR register.
// Requires BUSPROT::PPCKERN set if used from Broadway.
static inline void MEMCRMaskTrusted(MEMCRReg reg, u16 clear, u16 set)
{
    mask16(HW_BASE_TRUSTED + static_cast<u32>(reg), clear, set);
}

//  Mask MEMCR register in the untrusted Broadway mirror.
static inline void MEMCRMask(MEMCRReg reg, u16 clear, u16 set)
{
    mask16(HW_BASE + static_cast<u32>(reg), clear, set);
}