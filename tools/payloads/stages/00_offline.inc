/* STAGE 0. Built as: (none in the Makefile).
 * Included by trace.c inside payload_main(): it shares that function's locals, so it is a block, not yet a unit of its own.
 * To change it, edit it here; `make -C tools/payloads sums` must leave every other stage byte-identical. */
#if STAGE == 0
	ENV_SET("trognon", "console-1", 1);   /* console only: no vector touched */
#elif STAGE == 1
	ENV_SET("trognon", "hook-1", 1);
#elif STAGE == 2
	ENV_SET("trognon", "udf-1", 1);
#elif STAGE == 4
	ENV_SET("trognon", "pabt-1", 1);
#elif STAGE == 7
	ENV_SET("trognon", "irq-1", 1);
#elif STAGE == 8
	ENV_SET("trognon", "timer-1", 1);
#elif STAGE == 10
	ENV_SET("trognon", "tirq-1", 1);
#elif STAGE == 11
	ENV_SET("trognon", "tirqc-1", 1);
#elif STAGE == 12
	ENV_SET("trognon", "buttons-1", 1);
#elif STAGE == 13
	ENV_SET("trognon", "nand-1", 1);
#elif STAGE == 14
	ENV_SET("trognon", "nand-sweep-1", 1);
#elif STAGE == 15
	ENV_SET("trognon", "wifi-gpio-1", 1);
#elif STAGE == 16
	ENV_SET("trognon", "wifi-reset-1", 1);
#elif STAGE == 17
	ENV_SET("trognon", "sdio-dump-1", 1);
#elif STAGE == 18
	ENV_SET("trognon", "sdio-dump-2", 1);
#elif STAGE == 19
	ENV_SET("trognon", "sdio-cmd0-1", 1);
#elif STAGE == 20
	ENV_SET("trognon", "which-timer-1", 1);
#elif STAGE == 21
	ENV_SET("trognon", "which-timer-2", 1);
#elif STAGE == 22
	ENV_SET("trognon", "sdio-init-1", 1);
#elif STAGE == 23
	ENV_SET("trognon", "gate-write-1", 1);
#elif STAGE == 24
	ENV_SET("trognon", "write-test-1", 1);
#elif STAGE == 25
	ENV_SET("trognon", "dyn-clock16-1", 1);
#elif STAGE == 26
	ENV_SET("trognon", "iop-regs-1", 1);
#elif STAGE == 27
	ENV_SET("trognon", "iop-mmu-1", 1);
#elif STAGE == 28
	ENV_SET("trognon", "iop-clkgate-1", 1);
#elif STAGE == 29
	ENV_SET("trognon", "accel-i2c-1", 1);
#elif STAGE == 30
	ENV_SET("trognon", "bt-gpio-1", 1);
#elif STAGE == 31
	ENV_SET("trognon", "accel-axes-1", 1);
#elif STAGE == 32
	ENV_SET("trognon", "iop-vic-1", 1);
#elif STAGE == 33
	ENV_SET("trognon", "touch-gpio-1", 1);
#elif STAGE == 34
	ENV_SET("trognon", "compass-i2c-1", 1);
#elif STAGE == 35
	ENV_SET("trognon", "i2c-scan-1", 1);
#elif STAGE == 36
	ENV_SET("trognon", "pmu-read-1", 1);
#elif STAGE == 37
	ENV_SET("trognon", "spi1-pre-1", 1);
#elif STAGE == 38
	ENV_SET("trognon", "spi1-gate-1", 1);
#elif STAGE == 39
	ENV_SET("trognon", "pmu-w34-1", 1);
#elif STAGE == 40
	ENV_SET("trognon", "touch-power-dry-1", 1);
#elif STAGE == 41
	ENV_SET("trognon", "touch-rst-ctl-1", 1);
#elif STAGE == 42
	ENV_SET("trognon", "touch-ldo-only-1", 1);
#elif STAGE == 43
	ENV_SET("trognon", "gpio-survey-1", 1);
#elif STAGE == 44
	ENV_SET("trognon", "spi1-nogate-1", 1);
#elif STAGE == 45
	ENV_SET("trognon", "spi1-selftest-1", 1);
#elif STAGE == 46
	ENV_SET("trognon", "spi1-selftest-2", 1);
#elif STAGE == 47
	ENV_SET("trognon", "gate-survey-1", 1);
#elif STAGE == 48
	ENV_SET("trognon", "gate-snap-1", 1);
#elif STAGE == 5
	ENV_SET("trognon", "regs-1", 1);
#elif STAGE == 6
	ENV_SET("trognon", "vic-1", 1);
#else
	ENV_SET("trognon", "trace-1", 1);
#endif
