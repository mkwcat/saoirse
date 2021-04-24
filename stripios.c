/* 
 * Simple StripIOS - turn GCC output ELF into something IOS understands.
 * This is just to hold us off until Seeky does his job. 
 * 
 * Written by TheLordScruffy on April 24, 2021.
 * Copyright (C) 2021 TheLordScruffy - Do not distribute.
 * 
 * TODO: Make this compatible with IOS modules instead of just title code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#include "elf.h"


u16 bswap16(u16 v)
{
    return ((v & 0x00FF) << 8) | ((v & 0xFF00) >> 8);
}

u32 bswap32(u32 v)
{
    return ((v & 0x000000FF) << 24) | ((v & 0x0000FF00) << 8)
         | ((v & 0x00FF0000) >> 8) | ((v & 0xFF000000) >> 24);
}

u64 bswap64(u64 v)
{
    return ((v & 0x00000000000000FF) << 56) | ((v & 0x000000000000FF00) << 40)
         | ((v & 0x0000000000FF0000) << 24) | ((v & 0x00000000FF000000) << 8)
         | ((v & 0x000000FF00000000) >> 8) | ((v & 0x0000FF0000000000) >> 24)
         | ((v & 0x00FF000000000000) >> 40) | ((v & 0xFF00000000000000) >> 56);
}

u32 align_forward(u32 value, u32 align)
{
    return (value + align - 1) & (-1 ^ (align - 1));
}

u32 align_backward(u32 value, u32 align)
{
    return value & (-1 ^ (align - 1));
}


void swapEhdr(Elf32_Ehdr* ehdr)
{
    if (ehdr == NULL)
        return;
    ehdr->e_type       = bswap16(ehdr->e_type);
    ehdr->e_machine    = bswap16(ehdr->e_machine);
    ehdr->e_version    = bswap32(ehdr->e_version);
    ehdr->e_entry      = bswap32(ehdr->e_entry);
    ehdr->e_phoff      = bswap32(ehdr->e_phoff);
    ehdr->e_shoff      = bswap32(ehdr->e_shoff);
    ehdr->e_flags      = bswap32(ehdr->e_flags);
    ehdr->e_ehsize     = bswap16(ehdr->e_ehsize);
    ehdr->e_phentsize  = bswap16(ehdr->e_phentsize);
    ehdr->e_phnum      = bswap16(ehdr->e_phnum);
    ehdr->e_shentsize  = bswap16(ehdr->e_shentsize);
    ehdr->e_shnum      = bswap16(ehdr->e_shnum);
    ehdr->e_shstrndx   = bswap16(ehdr->e_shstrndx);
}

void swapShdr(Elf32_Shdr* shdr)
{
    if (shdr == NULL)
        return;
    shdr->sh_name      = bswap32(shdr->sh_name);
    shdr->sh_type      = bswap32(shdr->sh_type);
    shdr->sh_flags     = bswap32(shdr->sh_flags);
    shdr->sh_addr      = bswap32(shdr->sh_addr);
    shdr->sh_offset    = bswap32(shdr->sh_offset);
    shdr->sh_size      = bswap32(shdr->sh_size);
    shdr->sh_link      = bswap32(shdr->sh_link);
    shdr->sh_info      = bswap32(shdr->sh_info);
    shdr->sh_addralign = bswap32(shdr->sh_addralign);
    shdr->sh_entsize   = bswap32(shdr->sh_entsize);
}

void swapPhdr(Elf32_Phdr* phdr)
{
    if (phdr == NULL)
        return;
    phdr->p_type       = bswap32(phdr->p_type);
    phdr->p_offset     = bswap32(phdr->p_offset);
    phdr->p_vaddr      = bswap32(phdr->p_vaddr);
    phdr->p_paddr      = bswap32(phdr->p_paddr);
    phdr->p_filesz     = bswap32(phdr->p_filesz);
    phdr->p_memsz      = bswap32(phdr->p_memsz);
    phdr->p_flags      = bswap32(phdr->p_flags);
    phdr->p_align      = bswap32(phdr->p_align);

}


s32 main(s32 argc, char** argv)
{
    s32 i, j, k, l;
    u32 offset;
    bool success = false;
    const char *programName, *infile, *outfile;

    FILE *inf = NULL, *outf = NULL;
    long insize, outsize;
    Elf32_Ehdr iehdr, oehdr;
    Elf32_Shdr* ishdrs = NULL;
    Elf32_Phdr* ophdrs = NULL;
    void* temp = NULL;
    char* shstrtab = NULL;

    if (argc <= 0) return 1;
    if (argv == NULL) return 1;
    if (argv[0] == NULL) return 1;

    printf("Simple StripIOS - built " __DATE__ " at " __TIME__ ".\n");
    printf("Copyright (C) 2021 TheLordScruffy - Do not distribute.\n");

    programName = argv[0];

    if (argc != 3
     || argv[1] == NULL
     || argv[2] == NULL) {
        printf("Usage: %s <infile> <outfile>\n", programName);
        goto end;
    }

    infile = argv[1];
    outfile = argv[2];

    inf = fopen(infile, "rb");
    if (!inf) {
        fprintf(stderr, "[ERROR] Could not open infile: %s\n", infile);
        goto end;
    }
    if (fseek(inf, 0, SEEK_END) != 0
     || (insize = ftell(inf)) == -1
     || fseek(inf, 0, SEEK_SET) != 0
    ) {
        fprintf(stderr, "[ERROR] Unknown infile error (1).\n");
        goto end;
    }

    outf = fopen(outfile, "wb");
    if (!outf) {
        fprintf(stderr, "[ERROR] Could not open outfile: %s\n", outfile);
        goto end;
    }

    if (insize < sizeof(Elf32_Ehdr)
     || fread(&iehdr, sizeof(Elf32_Ehdr), 1, inf) != 1
     || memcmp(&iehdr, ELFMAG, SELFMAG)
     || iehdr.e_ident[EI_CLASS] != ELFCLASS32
     || iehdr.e_ident[EI_DATA] != ELFDATA2MSB
     || iehdr.e_ident[EI_VERSION] != EV_CURRENT
    ) {
        fprintf(stderr, "[ERROR] Invalid infile (1).\n");
        goto end;
    }

    swapEhdr(&iehdr);

    if (iehdr.e_type != ET_EXEC
     || iehdr.e_machine != 0x28
     || iehdr.e_version != 1
     || iehdr.e_shoff < sizeof(iehdr)
     || iehdr.e_shstrndx >= iehdr.e_shnum
     || (u64) iehdr.e_shoff + (u64) iehdr.e_shnum > insize
     || !(ishdrs = malloc(iehdr.e_shnum * sizeof(Elf32_Shdr)))
     || fseek(inf, iehdr.e_shoff, SEEK_SET) != 0
     || fread(ishdrs, iehdr.e_shnum * sizeof(Elf32_Shdr), 1, inf) != 1
    ) {
        fprintf(stderr, "[ERROR] Invalid infile (2).\n");
        goto end;
    }

    for (i = 0, j = 0; i < iehdr.e_shnum; i++)
    {
        swapShdr(&ishdrs[i]);

        if (ishdrs[i].sh_type == SHT_PROGBITS
         && (ishdrs[i].sh_offset < sizeof(Elf32_Ehdr)
         || (u64) ishdrs[i].sh_offset + (u64) ishdrs[i].sh_size > insize)
        ) {
            fprintf(stderr, "[ERROR] Invalid section header: %d\n", i);
            goto end;
        }

        if ((ishdrs[i].sh_type == SHT_PROGBITS
          || ishdrs[i].sh_type == SHT_NOBITS)
          && ishdrs[i].sh_size != 0) {
            j++;
         }
    }

    /* Setup output ELF. */
    memset(&oehdr, 0, sizeof(oehdr));

    oehdr.e_ident[EI_MAG0] = ELFMAG0;
    oehdr.e_ident[EI_MAG1] = ELFMAG1;
    oehdr.e_ident[EI_MAG2] = ELFMAG2;
    oehdr.e_ident[EI_MAG3] = ELFMAG3;
    oehdr.e_ident[EI_CLASS] = ELFCLASS32;
    oehdr.e_ident[EI_DATA] = ELFDATA2MSB;
    oehdr.e_ident[EI_VERSION] = EV_CURRENT;
    oehdr.e_ident[EI_OSABI] = 0x61; /* IOS? */

    oehdr.e_type = ET_EXEC;
    oehdr.e_machine = 0x28; /* ARM */
    oehdr.e_version = 1;
    oehdr.e_entry = iehdr.e_entry;
    oehdr.e_phoff = sizeof(Elf32_Ehdr);
    oehdr.e_flags = 0;
    oehdr.e_ehsize = sizeof(Elf32_Ehdr);
    oehdr.e_phentsize = sizeof(Elf32_Phdr);
    oehdr.e_phnum = j;
    oehdr.e_shentsize = sizeof(Elf32_Shdr);

    swapEhdr(&oehdr);
    if (fwrite(&oehdr, sizeof(oehdr), 1, outf) != 1) {
        fprintf(stderr, "[ERROR] ELF header write failed.\n");
        goto end;
    }

    ophdrs = calloc(j, sizeof(Elf32_Phdr));
    if (!ophdrs) {
        fprintf(stderr, "[ERROR] ophdrs alloc fail.\n");
        goto end;
    }

    offset = sizeof(oehdr) + j * sizeof(Elf32_Phdr);

    for (i = 1, j = 0; i < iehdr.e_shnum; i++)
    {
        if ((ishdrs[i].sh_type != SHT_PROGBITS
          && ishdrs[i].sh_type != SHT_NOBITS)
          || ishdrs[i].sh_size == 0)
            continue;

        ophdrs[j].p_type = 1;
        if (ishdrs[i].sh_type != SHT_NOBITS) {
            ophdrs[j].p_offset = offset;
            ophdrs[j].p_filesz = ishdrs[i].sh_size;
            offset += ophdrs[j].p_filesz;
        } else {
            ophdrs[j].p_offset = 0;
            ophdrs[j].p_filesz = 0;
        }
        ophdrs[j].p_vaddr = ishdrs[i].sh_addr;
        ophdrs[j].p_paddr = ishdrs[i].sh_addr;
        ophdrs[j].p_memsz = ishdrs[i].sh_size;
        ophdrs[j].p_flags = 0x07F00000;
        ophdrs[j].p_align = 4;

        swapPhdr(&ophdrs[j]);
        j++;
    }

    if (fwrite(ophdrs, sizeof(Elf32_Phdr) * j, 1, outf) != 1) {
        fprintf(stderr, "[ERROR] ELF program header write failed.\n");
        goto end;
    }

    for (i = 1, j = 0; i < iehdr.e_shnum; i++)
    {
        if (ishdrs[i].sh_type != SHT_PROGBITS
         || ishdrs[i].sh_size == 0)
            continue;

        temp = malloc(ishdrs[i].sh_size);
        if (!temp
         || fseek(inf, ishdrs[i].sh_offset, SEEK_SET) != 0
         || fread(temp, ishdrs[i].sh_size, 1, inf) != 1
        ) {
            fprintf(stderr, "[ERROR] Unknown infile error (2).\n");
            goto end;
        }

        if (fwrite(temp, ishdrs[i].sh_size, 1, outf) != 1) {
            fprintf(stderr, "[ERROR] ELF section write failed.\n");
            goto end;
        }

        free(temp);
        temp = NULL;
        j++;
    }

    success = true;

end:
    if (inf)
        fclose(inf);
    if (outf) {
        fclose(outf);
        if (!success)
            unlink(outfile);
    }
    if (ishdrs)
        free(ishdrs);
    if (ophdrs)
        free(ophdrs);
    if (shstrtab)
        free(shstrtab);
    if (temp)
        free(temp);

    return 0;
}