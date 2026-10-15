# 5. Field notes: what fails and how to tell

The dominant failure here is not a crash, it is a **silent success**: the system
answers, but with old code or a stale value. These notes exist to turn silent
failures into loud ones.

## USB flakiness (DEVICE)

Roughly 6 of about 18 payload sends failed with `USBError 5` (I/O error) or `60`
(timeout), then the device showed the Apple logo or stayed wedged. The same file
succeeded on retry every time so far. A single failure therefore says nothing about
the payload. Retry from a clean DFU before suspecting the code.

**Most of it was a cache bug, now fixed (2026-10-15).** `run` jumps to the payload address without flushing the
CPU caches. A payload sent after another one at the same address could execute the previous payload's instructions
and constants, and crash right after `run`: nearly every second send of a session failed. Every payload now begins
with a byte-identical stub (`hal/entry.c`) that cleans and invalidates the data cache over the payload area and
invalidates the instruction cache and branch predictor. Six payloads in a row then ran in 4 seconds, all with their
nonce confirmed (see [Evidence](08-evidence.md)). A failed pwn, or a send after a long idle, can still happen.

## Stale marker

The iBSS keeps the `trognon` variable across runs. After a failed send, `getenv`
returns the *previous* run's value. Always read it before the run (expect `<unset>`
after a fresh DFU) and check the screen: a payload repaints the whole display first,
so an old image on screen means the new payload did not run.

This is now checked by the tools instead of by eye: `tools/send.sh` appends a random nonce to the
binary, the payload republishes it, and a stale marker cannot match it (see
[boot chain](01-boot-chain.md)). Payloads that predate the nonce are reported as unconfirmed.

## Pwn failures

`ipwndfu -p` sometimes reports `Exploit failed`. Re-running it, sometimes after
reinstating DFU, works. It has not damaged the device.

## "Apple logo" means start over

The device left Recovery. The RAM payload and the pwned state are gone: DFU again.

## Emulation is necessary and not sufficient

Offline Unicorn runs caught real bugs before sending: GCC resolved `&&label` to the
function start (every handled exception restarted the payload); a flag left armed
after the first test would have re-entered a stale resume address. It cannot catch
what it does not model (`FSR`, `SCTLR.A`, caches, timing). A green emulation says the
control flow is right, nothing about the hardware.

## Stray lessons

- The iBSS `env_set()` does not preserve r4 to r11. Locals held in registers across the first call came
  back as garbage (a counter read 0, an elapsed time printed one value on screen and another over USB).
  `hal/result.c` now saves and restores those registers around the call. When a published number disagrees
  with the screen, suspect the call, not the hardware.
- Scratch RAM is not zeroed between payloads: initialise every flag.
- Reading by eye from a screen loses digits (we saw `41000000C` for `4100010C`).
  Keep the on-screen format regular and ask for a re-read when a value matters.
- Never copy a number from our own emulator into the docs as an observation. The emulator's fake
  `MIDR`/`SCTLR` (`0x412FC0F1`, `0x00C50078`) were published as DEVICE values and caught only when a
  photo of the real screen showed `0x412FC081` and `0x00C5187D`. Tag from the device reading, not
  from a render.
- A guess that was wrong is documented, not deleted: it is part of the material.
