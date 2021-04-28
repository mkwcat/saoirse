#--------------------#
# Shared definitions #
#--------------------#

# Build directory
TARGET := build

# Commands
STRIPIOS := ./stripios
BIN2S := bin2s
ELF2DOL := elf2dol

#--------------------------#
# title_loader definitions #
#--------------------------#

# Compiler
TL_CC := arm-none-eabi-gcc 
TL_CFLAGS := -std=c99 -march=armv5te -mbig-endian -mthumb-interwork -DDEBUG -fno-builtin -ffreestanding \
			 -n -nostartfiles -nodefaultlibs -Wl,-gc-sections -Os -s -fomit-frame-pointer -lgcc

# Source, include and build directories
TL_DIR := title_loader
TL_INCLUDES := $(TL_DIR)/lib
TL_TARGET := $(TARGET)/$(TL_DIR)

# Source files
TL_SOURCES := $(wildcard $(TL_DIR)/*.s) $(wildcard $(TL_DIR)/*.S) $(wildcard $(TL_DIR)/*.c) \
	$(wildcard $(TL_DIR)/lib/*.s) $(wildcard $(TL_DIR)/lib/*.S) $(wildcard $(TL_DIR)/lib/*.c)

# Output object files
TL_BIN := $(TARGET)/title_loader.o
TL_ELF := $(TARGET)/title_loader.elf

# Linker script
TL_LD = $(TL_DIR)/link.ld

#------------------------#
# elf_tester definitions #
#------------------------#

# Compiler
# TODO: remove hardcoded paths in flags?
ET_CC := powerpc-eabi-gcc
ET_CFLAGS := -O2 -Wall -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float -L C:/devkitPro/libogc/lib/wii \
			 -I C:/devkitPro/libogc/include -lwiiuse -lbte -logc -lm

# Source directory
ET_DIR := elf_tester

# Build directory
ET_TARGET := $(TARGET)/$(ET_DIR)

# Source files
ET_SOURCES := $(wildcard $(ET_DIR)/*.c) $(wildcard $(ET_DIR)/*.s) $(wildcard $(ET_DIR)/*.S)

# Output object files
TL_ELF_S := $(TL_ELF).s
ET_ELF := elf_tester.elf
ET_DOL := elf_tester.dol

#---------#
# Recipes #
#---------#

# Create build directory if it doesn't exist
DUMMY != mkdir -p build

# Define non-recipe commands
.PHONY: clean

# Make dol by default
default: $(ET_DOL)

$(TL_BIN):
	@echo $@: $(TL_SOURCES)
	@$(TL_CC) $(TL_CFLAGS) -T $(TL_LD) -I $(TL_INCLUDES) -o $@ $(TL_SOURCES)

$(TL_ELF): $(TL_BIN)
	@echo $@: $(TL_BIN)
	@$(STRIPIOS) $< $@

$(TL_ELF_S): $(TL_ELF)
	@echo $@: $<
	@$(BIN2S) $< >> $@

$(ET_ELF): $(TL_ELF_S)
	@echo $@: $<
	@$(ET_CC) $(ET_CFLAGS) $(ET_SOURCES) $(TL_ELF_S) -o $@

$(ET_DOL): $(ET_ELF) 
	@echo $@: $<
	@$(ELF2DOL) $< $@

clean:
	rm -f $(ET_DOL)
	rm -f $(ET_ELF)
	rm -rf build
