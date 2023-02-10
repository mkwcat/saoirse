# Clear the implicit built in rules
.SUFFIXES:

# Add .d to Make's recognized suffixes.
SUFFIXES += .d

# Project directory
BUILD         := build
PPC_SOURCES   := $(wildcard ppc/*) $(wildcard common/*)
PPC_INCLUDES  := -Ippc -Icommon
IOS_SOURCES   := $(wildcard ios/*) $(wildcard common/*)
IOS_INCLUDES  := -Iios -Icommon
BIN           := bin

# Target module names
TARGET_IOS_MODULE  := $(BUILD)/ios_module
TARGET_IOS_LOADER  := $(BUILD)/ios_loader
TARGET_PPC_CHANNEL := $(BUILD)/ppc_channel
TARGET_PPC_LOADER  := $(BUILD)/ppc_loader
TARGET_PPC_STUB    := $(BUILD)/boot

# Data archives
DATA_LOADER        := $(BUILD)/data/loader.arc.lzma

# Create build directories
DUMMY         != mkdir -p $(BIN) $(BUILD) \
                 $(foreach dir, $(PPC_SOURCES), $(BUILD)/$(dir)) \
		 $(foreach dir, $(IOS_SOURCES), $(BUILD)/$(dir)) \
		 $(DATA_LOADER).d

# Compiler definitions
PPC_PREFIX    := $(DEVKITPPC)/bin/powerpc-eabi-
PPC_CC        := $(PPC_PREFIX)gcc
PPC_LD        := $(PPC_PREFIX)ld
PPC_OBJCOPY   := $(PPC_PREFIX)objcopy

PPC_CFILES    := $(foreach dir, $(PPC_SOURCES), $(wildcard $(dir)/*.c))
PPC_CPPFILES  := $(foreach dir, $(PPC_SOURCES), $(wildcard $(dir)/*.cpp))
PPC_SFILES    := $(foreach dir, $(PPC_SOURCES), $(wildcard $(dir)/*.s))
PPC_OFILES    := $(PPC_CPPFILES:.cpp=.cpp.ppc.o) $(PPC_CFILES:.c=.c.ppc.o) \
		 $(PPC_SFILES:.s=.s.ppc.o)
PPC_OFILES    := $(addprefix $(BUILD)/, $(PPC_OFILES))

PPC_CHANNEL_LD := ppc_channel.ld
PPC_LOADER_LD := ppc_loader.ld
PPC_STUB_LD   := ppc_stub.ld

IOS_PREFIX    := $(DEVKITARM)/bin/arm-none-eabi-
IOS_CC        := $(IOS_PREFIX)gcc
IOS_LD        := $(IOS_PREFIX)ld
IOS_OBJCOPY   := $(IOS_PREFIX)objcopy

IOS_CFILES    := $(foreach dir, $(IOS_SOURCES), $(wildcard $(dir)/*.c))
IOS_CPPFILES  := $(foreach dir, $(IOS_SOURCES), $(wildcard $(dir)/*.cpp))
IOS_SFILES    := $(foreach dir, $(IOS_SOURCES), $(wildcard $(dir)/*.s))
IOS_OFILES    := $(IOS_CPPFILES:.cpp=.cpp.ios.o) $(IOS_CFILES:.c=.c.ios.o) \
		 $(IOS_SFILES:.s=.s.ios.o)
IOS_OFILES    := $(addprefix $(BUILD)/, $(IOS_OFILES))

IOS_MODULE_LD := ios_module.ld
IOS_LOADER_LD := ios_loader.ld

WUJ5          := tools/wuj5/wuj5.py
ELF2DOL       := tools/elf2dol.py
INCBIN        := tools/incbin.S

DEPS          := $(PPC_OFILES:.o=.d) $(IOS_OFILES:.o=.d)

# Compiler flags
WFLAGS   := -Wall -Wextra -Wpedantic -Wno-unused-const-variable \
	    -Wno-unused-function -Wno-pointer-arith -Wno-narrowing

CFLAGS   := $(INCLUDE) -O1 -fomit-frame-pointer \
            -fno-exceptions -fverbose-asm -ffunction-sections -fdata-sections \
            -fno-builtin-memcpy -fno-builtin-memset \
            $(WFLAGS)

CXXFLAGS := $(CFLAGS) -std=c++20 -fno-rtti

AFLAGS   := -x assembler-with-cpp

IOS_ARCH := -march=armv5te -mtune=arm9tdmi -mthumb-interwork -mbig-endian

IOS_LDFLAGS := $(IOS_ARCH) -lgcc -n -Wl,--gc-sections -Wl,-static

PPC_LDFLAGS := -flto -nodefaultlibs -nostdlib -n -Wl,--gc-sections -Wl,-static

IOS_DEFS := $(IOS_ARCH) $(IOS_INCLUDES) -DTARGET_IOS
PPC_DEFS := $(PPC_INCLUDES) -DTARGET_PPC

default: $(TARGET_PPC_LOADER).dol

clean:
	@echo Cleaning: $(BIN) $(BUILD)
	@rm -rf $(BIN) $(BUILD)

-include $(DEPS)

$(BUILD)/%.c.ppc.o: %.c
	@echo PPC: $<
	@$(PPC_CC) -MMD -MF $(BUILD)/$*.c.ppc.d $(CFLAGS) $(PPC_DEFS) -c -o $@ $<

$(BUILD)/%.cpp.ppc.o: %.cpp
	@echo PPC: $<
	@$(PPC_CC) -MMD -MF $(BUILD)/$*.cpp.ppc.d $(CXXFLAGS) $(PPC_DEFS) -c -o $@ $<

$(BUILD)/%.s.ppc.o: %.s
	@echo PPC: $<
	@$(PPC_CC) -MMD -MF $(BUILD)/$*.s.ppc.d $(AFLAGS) $(PPC_DEFS) -c -o $@ $<

$(BUILD)/%.c.ios.o: %.c
	@echo IOS: $<
	@$(IOS_CC) -MMD -MF $(BUILD)/$*.c.ios.d $(CFLAGS) $(IOS_DEFS) -c -o $@ $<

$(BUILD)/%.cpp.ios.o: %.cpp
	@echo IOS: $<
	@$(IOS_CC) -MMD -MF $(BUILD)/$*.cpp.ios.d $(CXXFLAGS) $(IOS_DEFS) -c -o $@ $<

$(BUILD)/%.s.ios.o: %.s
	@echo IOS: $<
	@$(IOS_CC) -MMD -MF $(BUILD)/$*.s.ios.d $(AFLAGS) $(IOS_DEFS) -c -o $@ $<

# PPC Channel

$(TARGET_PPC_CHANNEL).elf: $(PPC_OFILES) $(PPC_CHANNEL_LD)
	@echo Linking: $(notdir $@)
	@$(PPC_CC) -g -o $@ $(PPC_OFILES) -T$(PPC_CHANNEL_LD) $(PPC_LDFLAGS)

$(TARGET_PPC_CHANNEL).bin: $(TARGET_PPC_CHANNEL).elf
	@echo Output: $(notdir $@)
	@$(PPC_OBJCOPY) $< $@ -O binary

# PPC Loader

$(TARGET_PPC_LOADER).dol: $(TARGET_PPC_LOADER).elf
	@echo Output: $(notdir $@)
	@python $(ELF2DOL) $< $@

$(TARGET_PPC_LOADER).elf: $(PPC_OFILES) $(PPC_LOADER_LD) $(DATA_LOADER).o
	@echo Linking: $(notdir $@)
	@$(PPC_CC) -g -o $@ $(PPC_OFILES) $(DATA_LOADER).o -T$(PPC_LOADER_LD) $(PPC_LDFLAGS)

# IOS Module

$(TARGET_IOS_MODULE)_link.elf: $(IOS_OFILES) $(IOS_MODULE_LD)
	@echo Linking: $(notdir $@)
	@$(IOS_CC) -g $(IOS_LDFLAGS) $(IOS_OFILES) -T$(IOS_MODULE_LD) -o $@

$(TARGET_IOS_MODULE).elf: $(TARGET_IOS_MODULE)_link.elf
	@echo Output: $(notdir $@)
	@$(IOS_CC) -s $(IOS_LDFLAGS) $(IOS_OFILES) -T$(IOS_MODULE_LD) -Wl,-Map,$(TARGET_IOS_MODULE).map -o $@

# IOS Loader

$(TARGET_IOS_LOADER).elf: $(IOS_OFILES) $(IOS_LOADER_LD)
	@echo Linking: $(notdir $@)
	@$(IOS_CC) -g -o $@ $(IOS_OFILES) -T$(IOS_LOADER_LD) $(IOS_LDFLAGS)

$(TARGET_IOS_LOADER).bin: $(TARGET_IOS_LOADER).elf
	@echo Output: $(notdir $@)
	@$(IOS_OBJCOPY) $< $@ -O binary

# Data Archive

$(DATA_LOADER).d/ios_module.elf: $(TARGET_IOS_MODULE).elf
	@cp -f $< $@

$(DATA_LOADER).d/ios_loader.bin: $(TARGET_IOS_LOADER).bin
	@cp -f $< $@

$(DATA_LOADER).d/ppc_channel.bin: $(TARGET_PPC_CHANNEL).bin
	@cp -f $< $@

$(DATA_LOADER): $(DATA_LOADER).d/ios_module.elf \
                $(DATA_LOADER).d/ios_loader.bin \
		$(DATA_LOADER).d/ppc_channel.bin
	@rm -rf $@
	@echo Packing: $(notdir $@)
	@python $(WUJ5) encode $(DATA_LOADER).d

$(DATA_LOADER).o: $(DATA_LOADER) $(INCBIN)
	@echo PPC: $(notdir $(DATA_LOADER).o)
	@$(PPC_CC) $(INCBIN) -c -o $@ -DPATH=$(DATA_LOADER) -DNAME=LoaderArchive

