@echo off
cd title_loader
echo Compiling...
arm-none-eabi-gcc -std=c99 -march=armv5te -mbig-endian -mthumb-interwork -DDEBUG -T link.ld -fno-builtin -ffreestanding -n -nostartfiles -nodefaultlibs -I lib -Wl,-gc-sections -Os -fomit-frame-pointer start.S syscalls.S sao.c kernel.c lib/memcpy.s lib/string.c
mv a.out ../a.out
cd ..
stripios.exe a.out title_loader.elf
cd elf_tester
del title_loader.elf.s
bin2s ../title_loader.elf >> title_loader.elf.s
powerpc-eabi-gcc -O2 -Wall -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float -L C:/devkitPro/libogc/lib/wii -I C:/devkitPro/libogc/include -lwiiuse -lbte -logc -lm main.c es.bin.s title_loader.elf.s -o ../elf_tester.elf
cd ..
elf2dol elf_tester.elf elf_tester.dol
@echo on
