/*!
 * Author  : Star
 * Date    : 03 May 2021
 * File    : os.h
 * Version : 1.0.0.0
 */

#pragma once

//! Definitions
#ifdef IOS
#define OS_CACHED_BASE   0x00000000
#define OS_UNCACHED_BASE 0x80000000
#else
#define OS_CACHED_BASE   0x80000000
#define OS_UNCACHED_BASE 0xC0000000
#endif