// VirtualDisc.hpp - Emulated disc
//   Written by Palapeli
//
// Copyright (C) 2022 Team Saoirse
// SPDX-License-Identifier: MIT

#pragma once

#include <DVD/DI.hpp>
#include <System/Types.h>
#include <optional>

class VirtualDisc
{
public:
    virtual ~VirtualDisc();

    virtual bool UnencryptedRead(void* out, u32 wordOffset, u32 byteLen) = 0;
    virtual bool ReadFromPartition(void* out, u32 wordOffset, u32 byteLen) = 0;
    virtual DI::DIError OpenPartition(u32 wordOffset,
                                      ES::TMDFixed<512>* tmdOut) = 0;
    virtual bool ReadDiskID(DI::DiskID* out) = 0;
    virtual DI::DIError ReadTMD(ES::TMDFixed<512>* out) = 0;
    virtual bool IsInserted() = 0;
};