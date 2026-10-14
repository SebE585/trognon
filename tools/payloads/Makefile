# Payloads for the 3GS (Thumb, raw, loaded at 0x41000000). `make draw.png` renders offline.
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
CFLAGS  = -mthumb -mcpu=cortex-a8 -Os -ffreestanding -fno-builtin -fno-pic \
          -fno-tree-loop-distribute-patterns -nostdlib -Wall -Wextra

%.elf: %.c payload.ld
	$(CC) $(CFLAGS) -T payload.ld -o $@ $<

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

%.png: %.bin render_fb.py
	python3 -I render_fb.py $< $@

clean:
	rm -f *.elf *.bin *.png
.PHONY: clean
.SECONDARY:
