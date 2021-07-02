#pragma once

#include <types.h>

void* memcpy(void* dest, const void* src, u32 len);
s32 memcmp(const void* str1, const void* str2, u32 count);
void* memset(void *dest, int val, u32 len);
s32 strlen(const char* str);
s32 strcmp(const char* s1, const char* s2);

void* memcpy32(void* dest, const void* src, u32 len);
void* memset32(void* dest, int val, u32 len);
void memclear(void* ptr, u32 len);