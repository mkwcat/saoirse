// Disk.hpp - SD Card/USB I/O
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once
#include <FAT/ff.h>

extern FATFS fatfs;

namespace FSServ
{

bool MountSDCard();
bool UnmountSDCard();

}