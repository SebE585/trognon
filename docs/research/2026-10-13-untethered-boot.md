# Can A5-A11 iPhones boot a free OS without a host at every boot?

Research date: 2026-10-13. Web sources, not reproduced on hardware. Every claim
below is as reported by its source; "not verified" means we found no source.

## Bottom line

No public, documented way exists. Every working Linux-on-iPhone setup (A7-A11)
is fully tethered: checkm8 from a host, then pongoOS, then the kernel over USB. Two
theoretical routes to host-free boot: (a) jailbroken iOS + kloader on 32-bit
devices, (b) an always-attached microcontroller running checkm8. Neither has
been shown booting Linux publicly.

## 1. checkm8 is tethered by nature

- SecureROM bug in the USB DFU stack (CVE-2019-8900, axi0mX, 2019-09-27),
  A5-A11. Requires USB input on every boot.
  https://kb.cert.org/vuls/id/941987/, https://theapplewiki.com/wiki/Checkm8
- Linux on iPhone 7 (A10) via checkra1n and pongoOS, January 2020; kernel from
  RAM, unstable/limited.
  https://tuxphones.com/linux-kernel-boots-on-t8010-a10-iphone-7-via-pongoos/
- Ubuntu netbooted over NFS on an iPhone 7 with dead NAND, January 2021.
  https://gigazine.net/gsc_news/en/20210113-iphone-7-dead-nand-ubuntu-boot
- iPhone 5s (A7) Linux howto: host needed for exploit, pongoOS, kernel and
  netboot rootfs; no persistence. https://github.com/hmk3r/iphone-5s-linux-howto

## 2. 64-bit "semi-tethered" (palera1n)

- Semi-tethered = the phone boots stock, non-jailbroken iOS alone; jailbreak,
  pongoOS or custom code still need checkm8 from a host.
  https://github.com/palera1n/palera1n (A8-A11, iOS 15+)
- No source found loading pongoOS or Linux from a running jailbroken 64-bit
  iOS (kexec-style).
- kloader64 existed for iOS 7.0 to 8.4.1 on A7 (A8 needs own keys), dev-only, no
  evidence of booting Linux.
- Not verified: whether KPP/EL3 makes a proper 64-bit kexec impossible.

## 3. 32-bit (A5/A6): the only plausible host-free path

- kloader (winocm, ios-kexec-utils): loads an image into RAM and jumps to it
  from a jailbroken device (needs tfp0). https://theapplewiki.com/wiki/Kloader
- CoolBooter (2016, v1.6 October 2020): host-free dual-boot of another iOS on
  jailbroken 32-bit devices (iPhone 4/5/5c, iPad 2 to 4).
  https://theapplewiki.com/wiki/CoolBooter
- Untethered jailbreaks exist for all 32-bit devices on iOS 3.1.3 to 9.3.4
  (Legacy iOS Kit). https://theapplewiki.com/wiki/Legacy_iOS_Kit
- So untethered jailbreak + kloader + patched iBoot/iBEC + kernel is plausible.
  Not verified: no working Linux port for A5/A6 found. openiboot (2008)
  targeted pre-A4 hardware.
- iBoot exploits: De Rebus Antiquis (xerub, 2018, iOS 7 32-bit, untethered via
  HFS + NVRAM); iOS 5 HFS heap overflow (iPad 1). Both tied to old iOS
  versions, which need SHSH blobs to downgrade to.
  https://theapplewiki.com/wiki/De_Rebus_Antiquis
- Persistence: bootrom, LLB and iBoot check signatures and a per-device APTicket;
  nothing unsigned persists without a later-stage bug. SEP not researched.

## 4. Hardware dongle

- checkm8-a5: Arduino Uno + USB Host Shield, puts A5 devices into pwned DFU
  standalone. https://theapplewiki.com/wiki/Checkm8-a5
- Elcomsoft Raspberry Pi Pico firmware: applies checkm8 on iPhone 4S (May
  2022); automates DFU entry on iPhone 8/X (April 2023).
  https://blog.elcomsoft.com/?p=9665,
  https://blog.elcomsoft.com/2023/04/automating-dfu-mode-with-raspberry-pi-pico/
- Not found: any always-attached dongle that exploits, uploads pongoOS +
  kernel + initrd and boots unattended. Building blocks exist; open questions
  are DFU entry without button presses, USB host role, and image storage.
- usbliter8 (A12/A13, Pico 2): different bug, out of scope, not verified.

## Implication for Trognon

The dongle is the project's central open problem and its most original
contribution: nobody has published it.
