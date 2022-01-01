#pragma once
#include <System/Types.hpp>

class UIManager
{
private:
    static void calc();

public:
    static s32 threadEntry(void* arg);
};