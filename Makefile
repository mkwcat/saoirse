#----------------------#
# Clear built-in rules #
#----------------------#

.SUFFIXES:

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
TL_CFLAGS := -std=c99 -march=armv5te -mbig-endian -mthumb-interwork -mthumb -DDEBUG -fno-builtin -ffreestanding \
			 -ffunction-sections -n -nostartfiles -nodefaultlibs -Wl,-gc-sections -Os -fomit-frame-pointer -lgcc

# Source, include and build directories
TL_DIR := title_loader
TL_INCLUDES := -I $(TL_DIR)/include -I $(TL_DIR)/lib
TL_TARGET := $(TARGET)/$(TL_DIR)

# Source files
TL_SOURCES := $(wildcard $(TL_DIR)/*.s) $(wildcard $(TL_DIR)/*.S) $(wildcard $(TL_DIR)/*.c) \
	$(wildcard $(TL_DIR)/lib/*.s) $(wildcard $(TL_DIR)/lib/*.S) $(wildcard $(TL_DIR)/lib/*.c)

# Output object files
TL_OFILES := $(addsuffix .o, $(basename $(TL_SOURCES)))
TL_OFILES := $(subst $(TL_DIR), $(TL_TARGET), $(TL_OFILES))
TL_BIN := $(TARGET)/title_loader.o
TL_ELF := $(TARGET)/title_loader.elf

# Linker script
TL_LD = $(TL_DIR)/link.ld

#------------------------#
# elf_tester definitions #
#------------------------#

# Compiler
# TODO: use devkitpro environment variable for include paths
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
TL_ELF_S := $(TL_TARGET).s
ET_OFILES := $(addsuffix .o, $(basename $(ET_SOURCES)))
ET_OFILES := $(subst $(ET_DIR), $(ET_TARGET), $(ET_OFILES))
ET_ELF := elf_tester.elf
ET_DOL := elf_tester.dol

#---------#
# Recipes #
#---------#

# Create build directories if they don't exist
# TODO: don't hardcode subdirectories
DUMMY != mkdir -p $(TARGET) $(TL_TARGET) $(ET_TARGET) $(TL_TARGET)/lib

# Define non-recipe commands
.PHONY: clean

# Make dol by default
default: $(ET_DOL)

# Compile title_loader c, s and S files 
$(TL_TARGET)/%.o: $(TL_DIR)/%.*
	@echo $@: $<
	@$(TL_CC) $(TL_CFLAGS) $(TL_INCLUDES) -c -o $@ $<

# Link title_loader object files into one
$(TL_BIN): $(TL_OFILES)
	@echo $@: $^
	@$(TL_CC) $^ $(TL_CFLAGS) -T $(TL_LD) -o $@

# Strip title_loader o file
$(TL_ELF): $(TL_BIN)
	@echo $@: $(TL_BIN)
	@$(STRIPIOS) $< $@

# Dump title_loader ELF binary into s
$(TL_ELF_S): $(TL_ELF)
	@echo $@: $<
	@$(BIN2S) $< > $@

# Compile elf_tester c, s, and S files + dumped ELF
$(ET_TARGET)/%.o: $(ET_DIR)/%.*
	@echo $@: $<
	@$(ET_CC) $(ET_CFLAGS) -c -o $@ $<

# Link elf_tester object files into one
$(ET_ELF): $(TL_ELF_S) $(ET_OFILES)
	@echo $@: $^
	@$(ET_CC) $(ET_CFLAGS) $^ -o $@

# Convert elf_tester to dol
$(ET_DOL): $(ET_ELF) 
	@echo $@: $<
	@$(ELF2DOL) $< $@

# Remove build directory and products
clean:
	rm -f $(ET_DOL)
	rm -f $(ET_ELF)
	rm -rf build