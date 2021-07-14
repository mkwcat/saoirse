#pragma once

#include <util.h>
#include <types.h>

EXTERN_C_START

void* memcpy(void* dest, const void* src, u32 len);
s32 memcmp(const void* str1, const void* str2, u32 count);
void* memset(void *dest, int val, u32 len);
s32 strlen(const char* str);
s32 strnlen(const char* str, s32 maxlen);
s32 strcmp(const char* s1, const char* s2);
char* strchr(const char* s, char c);
char* strcpy(char* dst, const char* src);

void* memcpy32(void* dest, const void* src, u32 len);
void* memset32(void* dest, int val, u32 len);
void memclear(void* ptr, u32 len);

void usleep(u32 usec);

s32 snprintf(char* str, u32 n, const char* format, ...);

EXTERN_C_END