.PHONY: clean

BIN2S := bin2s

all:
	@$(MAKE) --no-print-directory -f ios.mk
	@$(MAKE) --no-print-directory -f channel.mk

clean:
	@rm -fr build_ios build_channel bin