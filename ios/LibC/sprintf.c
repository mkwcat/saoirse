#include <stdarg.h>
#include <stdio.h>

int sprintf(char* s, const char* format, ...)
{
    va_list arg;

    va_start(arg, format);
    int ret = vsprintf(s, format, arg);
    va_end(arg);

    return ret;
}

int snprintf(char* s, size_t n, const char* format, ...)
{
    va_list arg;

    va_start(arg, format);
    int ret = vsnprintf(s, n, format, arg);
    va_end(arg);

    return ret;
}
