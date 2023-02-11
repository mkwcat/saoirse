#include <Boot/Init.hpp>
#include <Debug/Console.hpp>
#include <Debug/Debug_VI.hpp>
#include <Import/RVL_OS.h>
#include <Import/Sel.h>

SelImport(
  "OSFatal", void OSFatal(u32* fgColor, u32* bgColor, const char* string));

SelImport("DVDLowReset", bool DVDLowReset(void* callback));
SelImport("DVDLowSetSpinupFlag", bool DVDLowSetSpinupFlag(bool flag));

SelImport("OSReturnToMenu", void OSReturnToMenu());

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    OSReport("saoirse main() called!\n");

    Debug_VI::Init();
    Console::Init();

    Console::Print("Printing from channel now\n");

#if 0
    DVDLowSetSpinupFlag(true);
    DVDLowReset(nullptr);
#endif

    OSReturnToMenu();

    while (true) {
    }
}

extern "C" void __eabi()
{
}

extern SelImportEntry SelImportTable;
extern SelImportEntry SelImportTableEnd;

ChannelInitInfo s_initInfo = {
  .entry = reinterpret_cast<u32>(&main),
  .importTable = &SelImportTable,
  .importTableEnd = &SelImportTableEnd,
};

__attribute__((section(".start"))) ChannelInitInfo* GetInitInfo()
{
    return &s_initInfo;
}
