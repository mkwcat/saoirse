#ifndef _IOS_HAXX_H
#define _IOS_HAXX_H

/* 
 * IOS Haxx
 * 
 * An IOS kernel patch that gives you full access to all mapped regions,
 * including read only and kernel regions.
 * 
 * Memory access:
 * FFF00000, size 0x0100000, kernel only
 * 13A70000, size 0x0020000, kernel only
 * 13AC0000, size 0x0020000, kernel only
 * 0D800000, size 0x00D0000, read only, uncached (hardware registers)
 * 00000000, size 0x4000000, read/write
 * 10000000, size 0x3600000, read/write
 * 13870000, size 0x0030000, read/write, uncached
 * 13600000, size 0x0020000, read/write
 * 13C40000, size 0x0080000, read/write
 * 13850000, size 0x0020000, read/write, uncached
 * 138F0000, size 0x00C0000, read/write
 * 
 * Any other regions will require disabling MMU
 */


/* Defined as syscall 0x3D */
s32 IOS_KernelExec(void* func, ...);

s32 IOS_Write32(u32 address, u32 value);
s32 IOS_Write16(u32 address, u16 value);
s32 IOS_Write8(u32 address, u8 value);
s32 IOS_Read32(u32 address);
s32 IOS_Read16(u32 address);
s32 IOS_Read8(u32 address);
void* IOS_memcpy(u32 dst, const void* src, u32 len);
s32 IOS_memclear(u32 dst, u32 len);

#endif /* _IOS_HAXX_H */