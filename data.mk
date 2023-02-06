#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
#---------------------------------------------------------------------------------
BUILD	    :=  build
ARCHIVE_D   :=  $(BUILD)/data/data.arc.lzma.d
TARGET      :=  data.arc
DATA        :=  data

#---------------------------------------------------------------------------------
# the prefix on the compiler executables
#---------------------------------------------------------------------------------
WUJ5 := tools/wuj5/wuj5.py

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
DUMMY != mkdir -p $(BUILD)/data $(ARCHIVE_D)

OUTPUT		:=  $(BIN)/$(TARGET)

default: $(OUTPUT)


$(ARCHIVE_D)/channel.bin: $(BUILD)/channel/channel.bin
	@cp -f $< $@

$(ARCHIVE_D)/ios_module.elf: $(BUILD)/ios/saoirse_ios.elf
	@cp -f $< $@

$(ARCHIVE_D)/ios_loader.bin: $(BUILD)/ios/ios_loader.bin
	@cp -f $< $@

$(OUTPUT): $(DATAFILES) $(ARCHIVE_D)/channel.bin $(ARCHIVE_D)/ios_module.elf $(ARCHIVE_D)/ios_loader.bin
	@rm -rf $@
	@echo packing ... $(notdir $@)
	@python $(WUJ5) encode $(ARCHIVE_D)


