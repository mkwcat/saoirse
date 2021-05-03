/*!
 * Author  : Star
 * Date    : 03 May 2021
 * File    : vi.h
 * Version : 1.0.0.0
 */

#pragma once

//! Definitions
#define VI_REG_BASE        0xCC002000
#define VI_VTR_REG_OFFSET  0x00000000
#define VI_TFBL_REG_OFFSET 0x0000001C
#define VI_HSW_REG_OFFSET  0x00000048

#define VI_TFBL_REG_PAGE_OFFSET_BIT ( 1 << 28 )