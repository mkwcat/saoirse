#pragma once
#include <ff.h>

extern FATFS fatfs;

namespace FSServ
{
bool MountSDCard();
bool UnmountSDCard();
}