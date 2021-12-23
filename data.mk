#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
#---------------------------------------------------------------------------------
BUILD		:=	build_channel
BIN			:=  bin
TARGET      :=  data.ar
DATA		:=  data

#---------------------------------------------------------------------------------
# the prefix on the compiler executables
#---------------------------------------------------------------------------------
PREFIX		:= $(DEVKITPPC)/bin/powerpc-eabi-
AR			:= $(PREFIX)ar

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
DUMMY != mkdir -p $(BIN) $(BUILD)
DUMMY != rm -rf $(BIN)/$(TARGET)

OUTPUT		:=  $(BIN)/$(TARGET)
DATAFILES := $(foreach dir,$(DATA),$(wildcard $(dir)/*))

default: $(OUTPUT)

$(OUTPUT): $(DATAFILES) $(BIN)/saoirse_ios.elf
	@echo packing ... $(notdir $@)
	@$(AR) -rc $@ $(DATAFILES) $(BIN)/saoirse_ios.elf


