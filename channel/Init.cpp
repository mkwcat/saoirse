#include <Boot/Init.hpp>

#define SelImport(_STR, _PROTOTYPE)                                            \
  asm("    .section .fimport_table\n"                                          \
      "    .global  __FIMPORT_ENTRY_" _STR "\n"                                \
      "__FIMPORT_ENTRY_" _STR ":\n"                                            \
      "    .long    __FIMPORT_STRING_" _STR "\n"                               \
      "    .long    __FIMPORT_STUB_" _STR "\n"                                 \
      "    .size    __FIMPORT_ENTRY_" _STR ", . - __FIMPORT_ENTRY_" _STR "\n"  \
      "\n"                                                                     \
      "    .section .rodata\n"                                                 \
      "__FIMPORT_STRING_" _STR ":\n"                                           \
      "    .string  \"" _STR "\"\n"                                            \
      "    .size    __FIMPORT_STRING_" _STR ", . - __FIMPORT_STRING_" _STR     \
      "\n\n"                                                                   \
      "    .section \".fimport." _STR "\", \"ax\"\n"                           \
      "__FIMPORT_STUB_" _STR ":\n"                                             \
      "    .p2align 2\n");                                                     \
  _Pragma("GCC diagnostic push");                                              \
  _Pragma("GCC diagnostic ignored \"-Wreturn-type\"");                         \
  __attribute__((section(".fimport." _STR))) __attribute__((weak)) _PROTOTYPE  \
  {                                                                            \
    asm volatile("nop");                                                       \
  }                                                                            \
  _Pragma("GCC diagnostic pop")

SelImport("OSReport", void OSReport(const char* format, ...));
SelImport(
  "OSFatal", void OSFatal(u32* fgColor, u32* bgColor, const char* string));

SelImport("DVDLowReset", bool DVDLowReset(void* callback));
SelImport("DVDLowSetSpinupFlag", bool DVDLowSetSpinupFlag(bool flag));

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    OSReport("saoirse main() called!\n");

    u32 fgColor = 0xFFFFFFFF;
    u32 bgColor = 0xFFF000FF;
    // OSFatal(&fgColor, &bgColor, "saoirse main() called!");

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
