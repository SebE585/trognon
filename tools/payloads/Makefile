# Payloads for the 3GS (Thumb, raw, loaded at 0x41000000). `make draw.png` renders offline.
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
CFLAGS  = -mthumb -mcpu=cortex-a8 -Os -ffreestanding -fno-builtin -fno-pic \
          -fno-tree-loop-distribute-patterns -nostdlib -Wall -Wextra

font.h: gen_font.py
	python3 -I gen_font.py $@

trace.elf trace_offline.elf: font.h

%.elf: %.c payload.ld
	$(CC) $(CFLAGS) -T payload.ld -o $@ $<

trace_s1.elf: trace.c payload.ld font.h
	$(CC) $(CFLAGS) -DSTAGE=1 -T payload.ld -o $@ $<

trace_s2.elf: trace.c payload.ld font.h
	$(CC) $(CFLAGS) -DSTAGE=2 -T payload.ld -o $@ $<

trace_pabt.elf: trace.c payload.ld font.h
	$(CC) $(CFLAGS) -DSTAGE=4 -T payload.ld -o $@ $<

trace_regs.elf: trace.c payload.ld font.h
	$(CC) $(CFLAGS) -DSTAGE=5 -T payload.ld -o $@ $<

trace_vic.elf: trace.c payload.ld font.h
	$(CC) $(CFLAGS) -DSTAGE=6 -T payload.ld -o $@ $<

trace_offline.elf: trace.c payload.ld font.h
	$(CC) $(CFLAGS) -DOFFLINE -T payload.ld -o $@ $<

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

%.png: %.bin render_fb.py
	python3 -I render_fb.py $< $@

clean:
	rm -f *.elf *.bin *.png
.PHONY: clean
.SECONDARY:
