# Homebrew MBR Program - 2019/01/07

This package contains the source code for the homebrew MBR program, which aims to function similarly to the Sony MBR program used for booting HDDOSD installations.

It supports the same integrity checks as the Sony MBR program and boots one of the following KELF targets:
- `pfs0:/osd/osdmain.elf` (Unofficial target, for FHDB)
- `pfs0:/osd/hosdsys.elf`
- `pfs0:/osd100/hosdsys.elf`

If the HDD is found to have filesystem corruption, FSCK in one of these locations will be launched, in this order:
- `pfs0:/fsck/fsck.elf` (Unofficial target, for FHDB)
- `pfs0:/fsck/fsck.elf`
- `pfs0:/fsck100/fsck.elf`

If all else fails, it will fall back to the OSD.

## Notes for Programmers

This MBR is split into two parts: the main program and an embedded EELOAD module, which is used to load the HDDOSD.

The embedded EELOAD module is hardcoded in the MBR program to expect the EELOAD module to run at `0x00084000`. Unlike `rom0:EELOAD` and various other EELOAD programs, the SDK used to build the MBR has the alarm functions patch (which resides at `0x00082000`).

The MBR program itself must run from `0x00100000` when decrypted by `rom0:HDDLOAD` or by a Sony OSD update.

Both programs are headerless, binary blobs of code.

Sony changed this behavior slightly for the PSBBN. On the surface, the documented behavior remained the same for the three options for HDDUNITPOWER (something similar is used within the MBR):
- **NICHDD**: The HDD's standby timer will be set to the Sony default of `0xFF` (21 minutes & 15 seconds).
- **NIC**: The HDD unit is put into IDLE state (it is usually still spinning).
- **Blank**: The network adaptor is switched off.

For the "NIC" mode, the HDD was put into IDLE state with the ATA IDLE command, but with a standby timer value of 0. Unfortunately, that also prevented the HDD from ever transitioning into STANDBY state (which could mean a spin down, depending on the HDD).

With the PSBBN, Sony issued the ATA IDLE IMMEDIATE command instead, which will only set the HDD into IDLE state without changing the standby timer. Thus, the HDD will be allowed to enter STANDBY state after 21 minutes and 15 seconds, as per the Sony default.
