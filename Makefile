.PHONY: clean

BIN2S := bin2s

all:
	@$(MAKE) -C ios
	@$(BIN2S) ios/bin/saoirse_ios.elf > test/source/saoirse_ios_elf.s
	@$(MAKE) -C test

clean:
	@$(MAKE) -C ios clean
	@$(MAKE) -C test clean