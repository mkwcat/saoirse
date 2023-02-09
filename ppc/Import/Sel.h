#pragma once

#define SelImport(_STR, _PROTOTYPE)                                            \
  asm("    .section .fimport_table\n"                                          \
      "    .weak    __FIMPORT_ENTRY_" _STR "\n"                                \
      "    .global  __FIMPORT_ENTRY_" _STR "\n"                                \
      "__FIMPORT_ENTRY_" _STR ":\n"                                            \
      "    .long    __FIMPORT_STRING_" _STR "\n"                               \
      "    .long    __FIMPORT_STUB_" _STR "\n"                                 \
      "    .size    __FIMPORT_ENTRY_" _STR ", . - __FIMPORT_ENTRY_" _STR "\n"  \
      "\n"                                                                     \
      "    .section .rodata\n"                                                 \
      "    .weak    __FIMPORT_STRING_" _STR "\n"                               \
      "    .global  __FIMPORT_STRING_" _STR "\n"                               \
      "__FIMPORT_STRING_" _STR ":\n"                                           \
      "    .string  \"" _STR "\"\n"                                            \
      "    .size    __FIMPORT_STRING_" _STR ", . - __FIMPORT_STRING_" _STR     \
      "\n\n"                                                                   \
      "    .section \".fimport." _STR "\", \"ax\"\n"                           \
      "    .weak    __FIMPORT_STUB_" _STR "\n"                                 \
      "    .global  __FIMPORT_STUB_" _STR "\n"                                 \
      "__FIMPORT_STUB_" _STR ":\n"                                             \
      "    .p2align 2\n");                                                     \
  _Pragma("GCC diagnostic push");                                              \
  _Pragma("GCC diagnostic ignored \"-Wreturn-type\"");                         \
  _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"");                    \
  __attribute__((section(".fimport." _STR))) __attribute__((weak)) _PROTOTYPE  \
  {                                                                            \
    /* A reference to the entry so garbage collect won't strip it */           \
    asm volatile("lwz 0, __FIMPORT_ENTRY_" _STR "@l(0)");                      \
  }                                                                            \
  _Pragma("GCC diagnostic pop")
