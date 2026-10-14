# 5. Field notes: what fails and how to tell

The dominant failure here is not a crash, it is a **silent success**: the system
answers, but with old code or a stale value. These notes exist to turn silent
failures into loud ones.

## USB flakiness (DEVICE)

Roughly 6 of about 18 payload sends failed with `USBError 5` (I/O error) or `60`
(timeout), then the device showed the Apple logo or stayed wedged. The same file
succeeded on retry every time so far. A single failure therefore says nothing about
the payload. Retry from a clean DFU before suspecting the code.

## Stale marker

The iBSS keeps the `trognon` variable across runs. After a failed send, `getenv`
returns the *previous* run's value. Always read it before the run (expect `<unset>`
after a fresh DFU) and check the screen: a payload repaints the whole display first,
so an old image on screen means the new payload did not run.

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

- Scratch RAM is not zeroed between payloads: initialise every flag.
- Reading by eye from a screen loses digits (we saw `41000000C` for `4100010C`).
  Keep the on-screen format regular and ask for a re-read when a value matters.
- A guess that was wrong is documented, not deleted: it is part of the material.
