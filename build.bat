@echo off
cd title_loader
echo Compiling...
arm-none-eabi-gcc -march=armv5te -mbig-endian -DDEBUG -T link.ld -fno-builtin -ffreestanding -n -nostartfiles -nodefaultlibs -Wl,-gc-sections -Os -s -fomit-frame-pointer start.S syscalls.S sao.c
mv a.out ../a.out
cd ..
stripios.exe a.out title_loader.elf
del a.out
@echo on
