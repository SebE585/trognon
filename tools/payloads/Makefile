# Payloads for the 3GS (Thumb, raw, loaded where the SoC header says). Portable code lives in hal/,
# per-chip constants in soc/, the payload itself in trace.c (see docs/en/07-code-layout.md).
#   make trace.bin trace_pabt.bin trace_regs.bin trace_vic.bin trace_irq.bin
#   make SOC=s5l8930 ALLOW_UNVERIFIED=1 ...    (refused otherwise: constants not verified on a device)
# `make draw.png`-style offline renders: python3 -I render_fb.py X.bin X.png
SOC     ?= s5l8920
SOCDEF  := -DSOC_$(shell echo $(SOC) | tr a-z A-Z)
ifdef ALLOW_UNVERIFIED
SOCDEF  += -DALLOW_UNVERIFIED
endif
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
CFLAGS  = -mthumb -mcpu=cortex-a8 -Os -ffreestanding -fno-builtin -fno-pic \
          -fno-tree-loop-distribute-patterns -nostdlib -Wall -Wextra \
          -ffunction-sections -I. $(SOCDEF)
# One source of truth for the load address: the SoC header.
PAYLOAD_BASE = $(shell echo SOC_PAYLOAD_BASE | $(CC) -E -P -x c -include soc.h $(SOCDEF) - 2>/dev/null | tr -d ' ')
LDFLAGS = -T payload.ld -Wl,--gc-sections -Wl,--defsym=PAYLOAD_BASE=$(PAYLOAD_BASE)
HAL     = hal/entry.o hal/console.o hal/exc.o hal/vic.o hal/result.o

# stage number per payload (STAGE is read by trace.c)
STAGE_trace      = 3
STAGE_trace_s1   = 1
STAGE_trace_s2   = 2
STAGE_trace_pabt = 4
STAGE_trace_regs = 5
STAGE_trace_vic  = 6
STAGE_trace_irq  = 7
STAGE_trace_timer = 8
STAGE_trace_ticks = 9
STAGE_trace_tirq  = 10
STAGE_trace_tirqc = 11
STAGE_trace_buttons = 12
STAGE_trace_nand    = 13
STAGE_trace_nand_sweep = 14
STAGE_trace_wifi_gpio  = 15
STAGE_trace_wifi_reset = 16
STAGE_trace_sdio_dump  = 17
STAGE_trace_sdio_dump_b = 18
STAGE_trace_sdio_cmd0  = 19
STAGE_trace_which_timer = 20
STAGE_trace_which_timer_b = 21
STAGE_trace_sdio_init  = 22
STAGE_trace_gate_write = 23
STAGE_trace_write_test = 24
STAGE_trace_dyn_clock16 = 25
STAGE_trace_iop_regs = 26
STAGE_trace_iop_probe_mmu = 27
STAGE_trace_iop_clkgate = 28

# Objects are built for one SoC. Without this stamp, `make SOC=other` would silently reuse objects built for
# the previous chip. The stamp is rewritten only when SOC or the flags change.
.build-stamp: FORCE
	@echo "$(SOC) $(SOCDEF) $(CFLAGS)" | cmp -s - $@ || echo "$(SOC) $(SOCDEF) $(CFLAGS)" > $@
FORCE:

font.h: gen_font.py
	python3 -I gen_font.py $@

hal/%.o: hal/%.c hal/*.h soc.h soc/*.h font.h .build-stamp
	$(CC) $(CFLAGS) -c -o $@ $<

trace.o: trace.c hal/*.h soc.h soc/*.h font.h .build-stamp
	$(CC) $(CFLAGS) -DSTAGE=$(STAGE_trace) -c -o $@ $<

trace_%.o: trace.c hal/*.h soc.h soc/*.h font.h .build-stamp
	$(CC) $(CFLAGS) -DSTAGE=$(STAGE_trace_$*) -c -o $@ $<

%.elf: %.o $(HAL) payload.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(HAL)

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

%.png: %.bin render_fb.py
	python3 -I render_fb.py $< $@

clean:
	rm -f *.o hal/*.o *.elf *.bin *.png .build-stamp
.PHONY: clean FORCE
.SECONDARY:
