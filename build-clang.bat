@echo off
cd title_loader
echo Compiling...
clang --target=arm-none-eabi -mbig-endian -nodefaultlibs -ffreestanding -fomit-frame-pointer -ffunction-sections -Wl,--gc-sections -Os -mcpu=arm926ej-s -T link.ld -I lib start.S syscalls.S sao.c kernel.c lib/string.c lib/memcpy.s
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
