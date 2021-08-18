#pragma once

#include <util.h>
#include <types.h>

constexpr u32 HW_BASE = 0x0D000000;
constexpr u32 HW_BASE_TRUSTED = 0x0D800000;

/* ACR (Hollywood Registers) */
enum class ACRReg
{
    IPC_PPCMSG   = 0x000,
    IPC_PPCCTRL  = 0x004,
    IPC_ARMMSG   = 0x008,
    IPC_ARMCTRL  = 0x00C,
    
    VISOLID      = 0x024,

    PPC_IRQFLAG  = 0x030,
    PPC_IRQMASK  = 0x034,
    ARM_IRQFLAG  = 0x038,
    ARM_IRQMASK  = 0x03C,

    /* Restricted Broadway GPIO access */
    GPIOB_OUT    = 0x0C0,
    GPIOB_DIR    = 0x0C4,
    GPIOB_IN     = 0x0C8,
    /* Full GPIO access */
    GPIO_OUT     = 0x0E0,
    GPIO_DIR     = 0x0E4,
    GPIO_IN      = 0x0E8,

    RESETS       = 0x194
};

inline u32 ACRReadTrusted(ACRReg reg) {
    return read32(HW_BASE_TRUSTED + static_cast<u32>(reg));
}
inline u32 ACRRead(ACRReg reg) {
    return read32(HW_BASE + static_cast<u32>(reg));
}
inline void ACRWriteTrusted(ACRReg reg, u32 value) {
    write32(HW_BASE_TRUSTED + static_cast<u32>(reg), value);
}
inline void ACRWrite(ACRReg reg, u32 value) {
    write32(HW_BASE + static_cast<u32>(reg), value);
}
inline void ACRMaskTrusted(ACRReg reg, u32 clear, u32 set) {
    mask32(HW_BASE_TRUSTED + static_cast<u32>(reg), clear, set);
}
inline void ACRMask(ACRReg reg, u32 clear, u32 set) {
    mask32(HW_BASE + static_cast<u32>(reg), clear, set);
}

/* GPIO pin connections */
enum class GPIOPin
{
    POWER        = 0x000001,
    SHUTDOWN     = 0x000002,
    FAN          = 0x000004,
    DC_DC        = 0x000008,
    DI_SPIN      = 0x000010,
    SLOT_LED     = 0x000020,
    EJECT_BTN    = 0x000040,
    SLOT_IN      = 0x000080,
    SENSOR_BAR   = 0x000100,
    DO_EJECT     = 0x000200,
    EEP_CS       = 0x000400,
    EEP_CLK      = 0x000800,
    EEP_MOSI     = 0x001000,
    EEP_MISO     = 0x002000,
    AVE_SCL      = 0x004000,
    AVE_SDA      = 0x008000,
    DEBUG0       = 0x010000,
    DEBUG1       = 0x020000,
    DEBUG2       = 0x040000,
    DEBUG3       = 0x080000,
    DEBUG4       = 0x100000,
    DEBUG5       = 0x200000,
    DEBUG6       = 0x400000,
    DEBUG7       = 0x800000
};

inline bool GPIOBRead(GPIOPin pin) {
    return static_cast<bool>
        (ACRRead(ACRReg::GPIOB_IN) & static_cast<u32>(pin));
}
inline void GPIOBWrite(GPIOPin pin, bool flag) {
    ACRMask(ACRReg::GPIOB_OUT, 0, flag ? static_cast<u32>(pin) : 0);
}
inline bool GPIORead(GPIOPin pin) {
    return static_cast<bool>
        (ACRReadTrusted(ACRReg::GPIO_IN) & static_cast<u32>(pin));
}
inline void GPIOWrite(GPIOPin pin, bool flag) {
    ACRMaskTrusted(ACRReg::GPIO_OUT, 0, flag ? static_cast<u32>(pin) : 0);
}
