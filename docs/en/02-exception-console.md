# 2. The on-screen exception console

The 3GS has no serial port we can reach and no debugger. When our code crashes the
phone reboots and tells nothing. So the first thing we built is a way to *see*.

## Memory layout used by the payloads (DEVICE)

| What | Address | Notes |
|---|---|---|
| Payload code | `0x41000000` | Thumb, loaded by `send_data` |
| Scratch struct | `0x41080000` | not zeroed: RAM keeps the previous payload's values |
| Handler stack top | `0x410f8000` | own stack for exception modes |
| Framebuffer | `0x4fd00000` | 320x480, RGB888 `0x00RRGGBB`, mapped cacheable write-back |
| Vector page | VA `0x0` | = iBSS image `0x84000000` |

The framebuffer is write-back: after drawing we clean the data cache so a frozen
CPU leaves a truthful screen. **Never read the framebuffer** (no scrolling).

## Hooking the vectors

Each vector is `ldr pc,[pc,#0x18]` through a pointer word: undef `0x24`,
SWI `0x28`, prefetch abort `0x2c`, data abort `0x30`, IRQ `0x38`, FIQ `0x3c`.
We overwrite the words in RAM and restore them at the end. Undef, prefetch and data
are proven on the device; the SWI, IRQ and FIQ offsets follow the same layout (**SOURCE**).

![How an exception becomes a readable screen](../img/exception-flow.svg)

The *breadcrumb bar* (a coloured bar at the bottom: red undef, green prefetch, blue data)
is drawn by assembly with no stack and no C, so even a crash inside the handler
shows which exception was entered.

## What is proven (DEVICE, 2026-10-15)

| Exception | Trigger | Observed |
|---|---|---|
| UNDEF | Thumb `udf #0` | `LR` = address of the `udf` + 2, `SPSR` with T=1, clean return |
| Data abort | `SCTLR.A` set, unaligned `ldr` | `FSR = 0x1` (alignment), `FAR` = the faulting address, clean return |
| Prefetch abort | Thumb `bkpt #0` | `IFSR = 0x2` (debug event), `LR` = bkpt + 4, clean return |

`IFAR` after a debug event is architecturally UNKNOWN; we do not rely on it.
Not yet tested: IRQ, FIQ, SWI. A `svc` from SVC mode would overwrite `SP_svc`/`LR_svc`,
so the SWI test needs SYS mode (**HYPOTHESIS**).

## Why an alignment fault

The live page table has no translation-fault entry (all sections 1..0xfff are
mapped) and every section is in domain 0 with `DACR = 3`, so neither a fault entry
nor a domain fault was available (DEVICE). Alignment faults are raised before any
bus access, so the test touches nothing.

## Reproduce

```sh
cd tools/payloads
make trace.bin trace_pabt.bin             # arm-none-eabi-gcc, Thumb
python3 -I render_fb.py trace.bin trace.png   # offline render with Unicorn
```

Emulation limits: Unicorn models none of `FSR`, `FAR`, `IFSR`, `SCTLR.A`, `DACR`.
Faults are simulated with unmapped addresses, so only the control flow is checked.
