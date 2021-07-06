// This makes LD avoid including the exception unwinder for long long division support
_Pragma("GCC diagnostic ignored \"-Wpedantic\"")
char __aeabi_unwind_cpp_pr0[0];
