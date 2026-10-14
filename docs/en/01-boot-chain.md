# 1. Boot chain: from DFU to our code

Goal: run code we wrote on a 3GS, with nothing written to the phone's storage.

## The path (DEVICE, 2026-10-14/15)

![Boot chain on the iPhone 3GS](../img/boot-chain.svg)

```mermaid
sequenceDiagram
    participant H as Host (Mac)
    participant R as SecureROM (DFU)
    participant I as iBSS (patched, in RAM)
    participant P as Our payload
    H->>R: ipwndfu -p (limera1n heap overflow)
    R-->>H: pwned DFU (signature checks off)
    H->>R: boot_ibss(): patched iBSS over DFU
    R->>I: jump, device re-enumerates as Recovery 0x1281
    H->>I: send_data(payload) to 0x41000000
    H->>I: send_command("run")
    I->>P: jump to 0x41000001 (Thumb)
    P-->>H: result via env variable (getenv)
```

1. **Pwned DFU.** The 3GS S5L8920 bootrom is vulnerable to limera1n. Host-direct,
   no helper board. The exploit fails some of the time and is simply repeated.
2. **Patched iBSS.** `boot_ibss()` from ipwndfu sends an iBSS decrypted with the
   device's own key and patched so the Recovery command `run` jumps to
   `0x41000001` instead of the stock behaviour. The Recovery command `go` is not used.
3. **Payload.** `send_data` copies raw Thumb code to `0x41000000`, `run` jumps to it.
   Entry state: MMU and caches on, vectors at VA 0 (the iBSS image at `0x84000000`),
   running in SVC mode, `SCTLR = 0x00C5187D`, IRQs not masked.
4. **Result channel.** The payload calls the iBSS internal `env_set()`
   (`0x84011799`, Thumb) to publish a marker, read back with `getenv`.
   **Trap:** the iBSS keeps the variable across runs. Read it unset *before* the run,
   otherwise a stale value proves nothing (see [field notes](05-field-notes.md)).

## What is not persistent

Nothing. The bootrom exploit and the RAM payload vanish at reboot (a logo means
the device left Recovery and must be put back in DFU). This is tethered boot.
Making it untethered on this SoC is **HYPOTHESIS**: see
[`../research/2026-10-13-untethered-boot.md`](../research/2026-10-13-untethered-boot.md).

## Reproduce

```sh
cd ~/tools/ipwndfu                       # a checkout of ipwndfu
export TROGNON_LIBUSB_PATH=/opt/homebrew/lib/libusb-1.0.dylib PYTHONPATH=.
python3 ipwndfu -p                        # repeat if "Exploit failed"
python3 trognon_boot_ibss_only.py         # local helper: boot_ibss() and stop
python3 <repo>/tools/run_payload.py <repo>/tools/payloads/trace.bin
```

`trognon_boot_ibss_only.py` is a 20-line local wrapper around ipwndfu's
`boot_ibss()`; it is not part of ipwndfu and not yet in this repository.
