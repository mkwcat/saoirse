#pragma once
#include <FAT/ff.h>

extern FATFS fatfs;

namespace FSServ
{

bool MountSDCard();
bool UnmountSDCard();

}