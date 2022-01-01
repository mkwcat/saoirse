#include "UIManager.hpp"
#include <ogc/video.h>

void UIManager::calc()
{
}

s32 UIManager::threadEntry([[maybe_unused]] void* arg)
{
    UIManager* obj = new UIManager;

    while (1) {
        obj->calc();
        VIDEO_WaitVSync();
    }
}