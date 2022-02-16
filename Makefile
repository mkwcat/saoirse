.PHONY: clean

BIN2S := bin2s

all:
	@$(MAKE) --no-print-directory -f ios.mk
	@$(MAKE) --no-print-directory -f data.mk
	@$(MAKE) --no-print-directory -f channel.mk
	@$(MAKE) --no-print-directory -f boot.mk

clean:
	@rm -fr build_ios build_channel build_boot bin