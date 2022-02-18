#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET		:=	saoirse_ios
BUILD		:=	build_ios
SOURCES		:=	$(wildcard ios/*) $(wildcard common/*)
DATA		:=	data  
INCLUDES	:=      ios common
BIN			:=  bin

LIBS		:=	
LIBDIRS		:=


#---------------------------------------------------------------------------------
# the prefix on the compiler executables
#---------------------------------------------------------------------------------
PREFIX		:= $(DEVKITARM)/bin/arm-none-eabi-
CC			:= $(PREFIX)gcc
CXX			:= $(PREFIX)g++
AR			:= $(PREFIX)ar
OBJCOPY		:= $(PREFIX)objcopy
LD			:= $(PREFIX)g++
AS			:= $(PREFIX)g++

#---------------------------------------------------------------------------------
# linker script
#---------------------------------------------------------------------------------
LINKSCRIPT		:= ios.ld
LINKSCRIPT_LOADER		:= ios_loader.ld

TARGET := saoirse_ios
LOADER_TARGET := ios_loader

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
DUMMY != mkdir -p $(BIN) $(BUILD) $(foreach dir,$(SOURCES),$(BUILD)/$(dir))

OUTPUT		:=  $(BIN)/$(TARGET)
LOADER_OUTPUT		:= $(BIN)/$(LOADER_TARGET)
CFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
sFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.s))
SFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.S))

OFILES		:=	$(CPPFILES:.cpp=_cpp.o) $(CFILES:.c=_c.o) \
					$(sFILES:.s=_s.o) $(SFILES:.S=_S.o)
OFILES		:= $(addprefix $(BUILD)/, $(OFILES))

DEPENDS		:=	$(addsuffix .d, $(basename $(OFILES)))

VPATH		=  $(foreach dir,$(SOURCES),$(ROOT)/$(dir))


#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
INCLUDE	:=			$(foreach dir,$(INCLUDES),-I$(dir))

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) 
LIBPATHS    :=  
LIBS        :=  -lgcc

ARCH	=	-march=armv5te -mtune=arm9tdmi -mthumb-interwork -mbig-endian

CFLAGS	=	$(ARCH) $(INCLUDE) -DTARGET_IOS -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-format-truncation -O3 -fomit-frame-pointer -fverbose-asm -ffunction-sections -fdata-sections -fno-exceptions
CXXFLAGS = $(CFLAGS) -std=c++20 -fno-rtti -fno-builtin-memcpy -fno-builtin-memset -Wno-narrowing

AFLAGS	=	$(ARCH) -x assembler-with-cpp

LDFLAGS	= $(ARCH) $(LIBPATHS) $(LIBS) -n -Wl,--gc-sections -Wl,-static -nostartfiles


default: $(OUTPUT).elf $(LOADER_OUTPUT).bin

clean:
	@echo cleaning...
	@rm -rf $(BIN) $(BUILD)
	
$(OUTPUT).elf: $(OUTPUT)_dbg.elf
	@echo output ... $(notdir $@)
	@$(LD) -s -o $@ $(OFILES) -T$(LINKSCRIPT) $(LDFLAGS) -Wl,-Map,$(BIN)/$(TARGET).map

$(OUTPUT)_dbg.elf: $(OFILES) $(LINKSCRIPT)
	@echo linking ... $(notdir $@)
	@$(LD) -g -o $@ $(OFILES) -T$(LINKSCRIPT) $(LDFLAGS)
	
$(LOADER_OUTPUT).bin: $(LOADER_OUTPUT).elf
	@echo output ... $(notdir $@)
	@$(OBJCOPY) $< $@ -O binary
	
$(LOADER_OUTPUT).elf: $(OFILES) $(LINKSCRIPT_LOADER)
	@echo linking ... $(notdir $@)
	@$(LD) -g -o $@ $(OFILES) -T$(LINKSCRIPT_LOADER) $(LDFLAGS)

$(BUILD)/%_cpp.o : %.cpp
	@echo $(notdir $<)
	@$(CXX) -g -MMD -MF $(BUILD)/$*_cpp.d $(CXXFLAGS) -c $< -o$@

$(BUILD)/%_c.o : %.c
	@echo $(notdir $<)
	@$(CC) -g -MMD -MF $(BUILD)/$*_c.d $(CFLAGS)  -c $< -o$@

$(BUILD)/%_s.o : %.s
	@echo $(notdir $<)
	@$(AS) -g -MMD -MF $(BUILD)/$*_s.d $(AFLAGS) -c $< -o$@

$(BUILD)/%_bin.o : %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)
	
define bin2o
	@echo  -e "\t.section .rodata\n\t.align 4\n\t.global $(*)\n\t.global $(*)_end\n$(*):\n\t.incbin \"$(subst /,\\\\\\\\,$(shell echo $< | sed 's=/==;s=/=:/='))\"\n$(*)_end:\n" > $@.s
	@$(CC)  $(ASFLAGS) $(AFLAGS) -c $@.s -o $@
	@rm -rf $@.s
endef
