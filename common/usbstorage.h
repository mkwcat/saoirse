#pragma once

#include <types.h>

namespace USBStorage
{

namespace Error {
  enum {
    OK = 0,
    NoInterface = -10000,
    Sense = -10001,
    ShortWrite = -10002,
    ShortRead = -10003,
    Signature = -10004,
    Tag = -10005,
    Status = -10006,
    DataResidue = -10007,
    Timedout = -10008,
    Init = -10009,
    Processing = -10010
  };
}

bool Startup();
bool ReadSectors(u32 sector, u32 numSectors, void* buffer);
bool WriteSectors(u32 sector, u32 numSectors, const void* buffer);
void Shutdown();

}