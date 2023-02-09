#include <Boot/Init.hpp>
#include <Import/RVL_OS.h>
#include <Import/Sel.h>

SelImport(
  "OSFatal", void OSFatal(u32* fgColor, u32* bgColor, const char* string));

SelImport("DVDLowReset", bool DVDLowReset(void* callback));
SelImport("DVDLowSetSpinupFlag", bool DVDLowSetSpinupFlag(bool flag));

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    OSReport("saoirse main() called!\n");

    DVDLowSetSpinupFlag(true);
    DVDLowReset(nullptr);

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
