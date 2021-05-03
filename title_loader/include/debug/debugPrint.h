/*!
 * Author  : Star
 * Date    : 03 May 2021
 * File    : debugPrint.h
 * Version : 1.0.0.0
 */

#pragma once

#include <revolution/gx.h>

//! Function Prototype Declarations

//! Public Functions
int DebugPrint_Printf(int iCurrentRow, int iCurrentColumn, const char* pFormatString, ...);

void DebugPrint_SetForegroundColour(GXColor gxColor);
void DebugPrint_SetBackgroundColour(GXColor gxColor);