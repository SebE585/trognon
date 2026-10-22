# ipwndfu, pinned and patched

Trognon does not vendor ipwndfu. It drives it, at a pinned version, with a small patch.

| | |
|---|---|
| Upstream | https://github.com/axi0mX/ipwndfu |
| Pinned commit | `0e28932ec6a2a570b10fd77e50bda4216418cd98` |
| Our patch | `python3-arm64.patch`: Python 3 port of the files we use, an arm64 macOS libusb path override (`TROGNON_LIBUSB_PATH`), no change to any exploit logic we are aware of |
| Helper | `boot_ibss_only.py`: calls `boot_ibss()` and stops there (RAM only, never `flash_nor()`) |

The patch is a derivative of ipwndfu and carries its license, **GPL-3.0**. It is kept in this directory
and is not part of Trognon's own GPL-2.0-only or MIT code.

## Set up (once)

```sh
git clone https://github.com/axi0mX/ipwndfu.git ~/tools/ipwndfu
cd ~/tools/ipwndfu
git checkout 0e28932ec6a2a570b10fd77e50bda4216418cd98
git apply <trognon-repo>/tools/ipwndfu/python3-arm64.patch
cp <trognon-repo>/tools/ipwndfu/boot_ibss_only.py .
```

Needs Python 3 with `pyusb` and a libusb 1.0 (`brew install libusb`). Set `TROGNON_LIBUSB_PATH` to its
`.dylib` if it is not found (`tools/boot.sh` does it for Homebrew on Apple silicon).

## Use

```sh
tools/boot.sh                       # pwn the device in DFU, then boot the patched iBSS (Recovery)
tools/send.sh tools/payloads/trace.bin     # send a payload, check its nonce
```

`tools/boot.sh` stops and says so when the checkout is not at the pinned commit, so a different ipwndfu
cannot be used by accident. A failed pwn is common: it is repeated at most three times, and the device
must be put back in DFU between failures (the script says when).

## A stuck USB connection, without touching the device (2026-10-22)

A device still visible in `ioreg` (macOS, no USB traffic) but failing every pyusb call with
`ValueError: The device has no langid` or a control-transfer `Operation timed out` is a host-side USB state
stuck, not a dead or disconnected device. `device.reset()` (`usb.core.Device.reset()`, wraps
`libusb_reset_device()`) asks for a USB-level reset -- re-enumeration -- without a power cycle: an already
pwned device stays pwned across it. Found live this session (confirmed: the same symptom reproduces on a
known-good read too, so it is general host-side flakiness, not a bad address or command) after a physical
cable replug had already been the only known fix. `tools/ipwndfu/read_chip_revision.py`'s own
`recover_with_usb_reset()` is a reusable example of the pattern: try the call once, and only on exactly this
symptom, reset and retry before falling back to a physical replug.
