# XenoBoot v2 - memory card DOL loader (payload embedded in the XenoGC ATmega8
# firmware).  Needs devkitPPC.  Run from this directory:  make
#
#   build/XenoShell.bin        the real loader, linked at 0x81500000
#   build/XenoShell.lz         LZSS compressed loader (tools/lzss.py)
#   build/XenoBoot_packed.bin  decompressor stub + compressed loader, linked at
#                              0x81700000 -> this is what goes into XenoAT
DEVKITPPC ?= /opt/devkitpro/devkitPPC
CC      = $(DEVKITPPC)/bin/powerpc-eabi-gcc
OBJCOPY = $(DEVKITPPC)/bin/powerpc-eabi-objcopy
SIZE    = $(DEVKITPPC)/bin/powerpc-eabi-size
SRC     = source
BUILD   = build
# -fno-builtin/-fno-tree-loop-distribute-patterns: no libc, so gcc must not emit memset/memcpy calls
CFLAGS  = -nostdlib -nostartfiles -Wall -Os -pipe -fno-builtin -fno-tree-loop-distribute-patterns \
          -ffunction-sections -fdata-sections \
          -fno-asynchronous-unwind-tables -fno-stack-protector -msoft-float -fno-exceptions \
          -Wa,-mregnames -Wl,--gc-sections -Wl,-s

# hard limit: ATmega8 flash (8192) minus AVR code + drivecode + uploader (0xF50) minus 2 length bytes
PAYLOAD_MAX = 4270

all: $(BUILD)/XenoBoot_packed.bin

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/XenoShell.bin: $(SRC)/boot.s $(SRC)/main.c $(SRC)/main.h $(SRC)/cardmath.h $(SRC)/font5x7.h $(SRC)/link.lds | $(BUILD)
	$(CC) $(CFLAGS) -Wl,-T,$(SRC)/link.lds -Wl,-Map=$(BUILD)/XenoShell.map $(SRC)/boot.s $(SRC)/main.c -lgcc -o $(BUILD)/XenoShell.elf
	$(OBJCOPY) -O binary $(BUILD)/XenoShell.elf $@

$(BUILD)/XenoShell.lz: $(BUILD)/XenoShell.bin ../tools/lzss.py
	python3 ../tools/lzss.py $< $@

$(BUILD)/payload_len.s: $(BUILD)/XenoShell.bin
	printf '.section .rodata\n.balign 4\n.global payload_len\npayload_len:\n\t.long %d\n' $$(stat -f%z $< 2>/dev/null || stat -c%s $<) > $@

$(BUILD)/XenoBoot_packed.bin: $(SRC)/stub.s $(SRC)/stub.c $(SRC)/lzdec.h $(SRC)/stub.lds $(BUILD)/XenoShell.lz $(BUILD)/payload_len.s
	$(CC) $(CFLAGS) -Wl,-T,$(SRC)/stub.lds $(SRC)/stub.s $(BUILD)/payload_len.s $(SRC)/stub.c -o $(BUILD)/stub.elf
	$(OBJCOPY) -O binary $(BUILD)/stub.elf $@
	@sz=$$(stat -f%z $@ 2>/dev/null || stat -c%s $@); echo "packed payload: $$sz bytes (limit $(PAYLOAD_MAX))"; \
	 if [ $$sz -gt $(PAYLOAD_MAX) ]; then echo "ERROR: payload too large for the ATmega8 flash"; rm -f $@; exit 1; fi

clean:
	rm -rf $(BUILD)
