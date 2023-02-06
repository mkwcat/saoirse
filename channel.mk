# Add .d to Make's recognized suffixes.
SUFFIXES += .d

# Project directory
BUILD := build/channel
TARGET := channel
LOADER := Loader

# Compiler definitions
CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
LD := $(DEVKITPPC)/bin/powerpc-eabi-ld
OBJCOPY := $(DEVKITPPC)/bin/powerpc-eabi-objcopy

CFILES := $(wildcard channel/*.c)
CPPFILES := $(wildcard channel/*.cpp)
SFILES := $(wildcard channel/*.s)
OFILES		:=	$(CPPFILES:.cpp=_cpp.o) $(CFILES:.c=_c.o) $(SFILES:.s=_s.o)
OFILES		:= $(addprefix $(BUILD)/, $(OFILES))
DEPS	:= $(OFILES:.o=.d)

DUMMY != mkdir -p $(BUILD) $(BUILD)/channel

CFLAGS := -O3 -fno-rtti -fno-short-enums -fshort-wchar -fno-exceptions -nodefaultlibs -ffreestanding -ffunction-sections -fdata-sections -Icommon -Ichannel


default: $(BUILD)/$(TARGET).bin

clean:
	@echo cleaning...
	@rm -rf $(BUILD)

-include $(DEPS)

$(BUILD)/%_c.o: %.c
	@echo $<
	@$(CC) -MMD $(CFLAGS) -I../include -c -o $@ $<

$(BUILD)/%_cpp.o: %.cpp
	@echo $<
	@$(CC) -std=c++17 -MMD $(CFLAGS) -I../include -c -o $@ $<

$(BUILD)/%_s.o: %.s
	@echo $<
	@$(CC) -x assembler-with-cpp -I../include -c -o $@ $<

$(BUILD)/$(TARGET).elf: $(OFILES)
	@echo linking ... $(TARGET).elf
	@$(CC) -Tchannel.ld -n $(OFILES) -Wl,--gc-sections -Wl,-static -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	@echo output ... $(notdir $@)
	@$(OBJCOPY) $< $@ -O binary
