#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

PREFIX		:= $(DEVKITPPC)/bin/powerpc-eabi-
CC			:= $(PREFIX)gcc
CXX			:= $(PREFIX)g++
AR			:= $(PREFIX)ar
OBJCOPY		:= $(PREFIX)objcopy
LD			:= $(PREFIX)g++
AS			:= $(PREFIX)as

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET		:=	boot
BUILD		:=	build/boot
BIN         :=  bin
SOURCES		:=	boot
INCLUDES	:=	common

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
INCLUDE	:=	$(foreach dir,$(INCLUDES), -I$(dir))

CFLAGS	= -g -O4 -Wall -ffreestanding $(INCLUDE)
CXXFLAGS	=	$(CFLAGS) -fno-exceptions -fno-unwind-tables -fno-omit-frame-pointer

AFLAGS = -x assembler-with-cpp $(INCLUDE)

LDFLAGS	=	-g -Wl,--gc-sections -Wl,-Map,$(BIN)/$(notdir $@).map -ffreestanding -nodefaultlibs -nostdlib

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:= -lm -lgcc

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
DUMMY != mkdir -p $(BUILD) $(BIN) $(foreach dir,$(SOURCES),$(BUILD)/$(dir))

LIBDIRS	:=

OUTPUT	:=	$(TARGET)

VPATH	:=	$(foreach dir,$(SOURCES),$(dir)) \
					$(foreach dir,$(DATA),$(dir))

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
sFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.s))
SFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.S))

OFILES		:=	$(CPPFILES:.cpp=_cpp.o) $(CFILES:.c=_c.o) \
					$(sFILES:.s=_s.o) $(SFILES:.S=_S.o)
OFILES		:= $(addprefix $(BUILD)/, $(OFILES))

LINKSCRIPT = boot.ld

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					-L$(LIBOGC_LIB)

OUTPUT	:=	$(CURDIR)/$(BIN)/$(TARGET)
.PHONY: $(BUILD) clean

default: $(OUTPUT).dol

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol

#---------------------------------------------------------------------------------
run:
	wiiload $(BIN)/$(TARGET).dol

#---------------------------------------------------------------------------------


DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).dol: $(OUTPUT).elf
	@echo output ... $(notdir $@)
	@python tools/elf2dol.py $< $@

$(OUTPUT).elf: $(OFILES) $(LINKSCRIPT) $(BUILD)/data.arc.lzma.o
	@echo linking ... $(notdir $@)
	@$(LD) -g -o $@ $(OFILES) $(BUILD)/data.arc.lzma.o -T$(LINKSCRIPT) $(LDFLAGS)

$(BUILD)/%_cpp.o : %.cpp
	@echo $(notdir $<)
	@$(CC) -g -MMD -MF $(BUILD)/$*_cpp.d $(CXXFLAGS) -c $< -o$@

$(BUILD)/%_c.o : %.c
	@echo $(notdir $<)
	@$(CC) -g -MMD -MF $(BUILD)/$*_c.d $(CFLAGS)  -c $< -o$@

$(BUILD)/%_s.o : %.s
	@echo $(notdir $<)
	@$(CC) -g -MMD -MF $(BUILD)/$*_s.d -x assembler-with-cpp -c $< -o$@

-include $(DEPENDS)

#---------------------------------------------------------------------------------
# This rule links in the data archive
#---------------------------------------------------------------------------------
$(BUILD)/data.arc.lzma.o	:	$(BUILD)/../data/data.arc.lzma
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
# canned command sequence for binary data
#---------------------------------------------------------------------------------
define bin2o
	$(SILENTCMD)bin2s -a 32 $< | $(AS) -o $@
endef
