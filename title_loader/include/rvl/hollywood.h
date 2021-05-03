#ifndef _HOLLYWOOD_H
#define _HOLLYWOOD_H

#define BROADWAY_BASE 0x0D000000
#define STARLET_BASE  0x0D800000

#define HW_IPC_PPCMSG    STARLET_BASE + 0x0
#define HW_IPC_PPCCTRL   STARLET_BASE + 0x4
#define HW_IPC_ARMMSG    STARLET_BASE + 0x8
#define HW_IPC_ARMCTRL   STARLET_BASE + 0xC

#define HW_VIDIM         STARLET_BASE + 0x1C
#define HW_VISOLID       STARLET_BASE + 0x24

#define HW_SRNPROT       STARLET_BASE + 0x60
#define HW_AHBPROT       STARLET_BASE + 0x64
#define HW_AIPPROT       STARLET_BASE + 0x70

#define HW_GPIO_ENABLE   STARLET_BASE + 0xDC
#define HW_GPIO_OUT      STARLET_BASE + 0xE0
#define HW_GPIO_DIR      STARLET_BASE + 0xE4
#define HW_GPIO_IN       STARLET_BASE + 0xE8
#define HW_GPIO_INTLVL   STARLET_BASE + 0xEC
#define HW_GPIO_INTFLAG  STARLET_BASE + 0xF0
#define HW_GPIO_INTMASK  STARLET_BASE + 0xF4
#define HW_GPIO_STRAPS   STARLET_BASE + 0xF8
#define HW_GPIO_OWNER    STARLET_BASE + 0xFC

/* GPIO pin connections */
#define GPIO_POWER       0x000001
#define GPIO_SHUTDOWN    0x000002
#define GPIO_FAN         0x000004
#define GPIO_DC_DC       0x000008
#define GPIO_DI_SPIN     0x000010
#define GPIO_SLOT_LED    0x000020
#define GPIO_EJECT_BTN   0x000040
#define GPIO_SLOT_IN     0x000080
#define GPIO_SENSOR_BAR  0x000100
#define GPIO_DO_EJECT    0x000200
#define GPIO_EEP_CS      0x000400
#define GPIO_EEP_CLK     0x000800
#define GPIO_EEP_MOSI    0x001000
#define GPIO_EEP_MISO    0x002000
#define GPIO_AVE_SCL     0x004000
#define GPIO_AVE_SDA     0x008000
#define GPIO_DEBUG0      0x010000
#define GPIO_DEBUG1      0x020000
#define GPIO_DEBUG2      0x040000
#define GPIO_DEBUG3      0x080000
#define GPIO_DEBUG4      0x100000
#define GPIO_DEBUG5      0x200000
#define GPIO_DEBUG6      0x400000
#define GPIO_DEBUG7      0x800000

#define HW_RESETS        STARLET_BASE + 0x194

/* HW_RESETS fields */
#define RSTBINB          0x0000001 /* System reset */
#define CRSTB            0x0000002 /* CRST reset? */
#define RSTB             0x0000004 /* RSTB reset? */
#define RSTB_DSKPLL      0x0000008 /* DSKPLL reset */
#define CPURSTB          0x0000010 /* PowerPC HRESET */
#define CPUSRSTB         0x0000020 /* PowerPC SRESET */
#define RSTB_SYSPLL      0x0000040 /* SYSPLL reset */
#define NLCKB_SYSPLL     0x0000080 /* Unlock SYSPLL reset? */
#define RSTB_MEMRSTB     0x0000100 /* MEM reset B */
#define RSTB_PI          0x0000200 /* PI reset */
#define RSTB_DIRSTB      0x0000400 /* Drive Interface reset B */
#define RSTB_MEM         0x0000800 /* MEM reset */
#define RSTB_GFXTCPE     0x0001000 /* GFX TCPE? */
#define RSTB_GFX         0x0002000 /* GFX reset? */
#define RSTB_AI_I2S3     0x0004000 /* Audio Interface I2S3 reset */
#define RSTB_IOSI        0x0008000 /* Serial Interface I/O reset */
#define RSTB_IOEXI       0x0010000 /* External Interface I/O reset */
#define RSTB_IODI        0x0020000 /* Drive Interface I/O reset */
#define RSTB_IOMEM       0x0040000 /* MEM I/O reset */
#define RSTB_IOPI        0x0080000 /* Processor Interface I/O reset */
#define RSTB_VI          0x0100000 /* Video Interface reset */
#define RSTB_VI1         0x0200000 /* VI1 reset? */
#define RSTB_IOP         0x0400000 /* IOP reset */
#define RSTB_AHB         0x0800000 /* ARM AHB reset */
#define RSTB_EDRAM       0x1000000 /* External DRAM reset */
#define NLCKB_EDRAM      0x2000000 /* Unlock external DRAM reset? */

#endif // _HOLLYWOOD_H