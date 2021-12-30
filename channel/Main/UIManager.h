#pragma once
#include <types.h>

class UIManager
{
private:
    static void calc();

public:
    static s32 threadEntry(void* arg);
};