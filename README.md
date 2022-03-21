# OSD-Initialization-Libraries

Forked from <https://sites.google.com/view/ysai187/home/projects/initializing-the-ps2psx>
Originally posted here: <https://assemblergames.com/threads/initializing-the-playstation-2-from-cold-boot.67794/>

## Introduction

This is a write-up to share about how the PS2 is to be initialized from within a memory-card update program.

However, a write-up is still based on reverse-engineering and updating code from the FMCB, so that it will be more usable for external projects.
The OSD init stuff is consolidated into a new library, named `libosdinit`. It may make things easier for new software to work with the OSD configuration.

Although we have various samples floating around the Internet in the form of the FreeVast and FMCB v1.7 source code, FMCB v1.8 did not quite have an accurate method of initializing the PS2.

Basics of initializing SIFRPC/SIFCMD services will not be covered here, as those are the basics required for each project.

## Initializing the PS2

Things that need to be done:

- Initialize SIFCMD/SIFRPC.
- Reboot the IOP.
  - While homebrew software generally uses the ROM modules and things may work without this step, it does not result in universal behavior across all console models. Depending on the ROM version, most PlayStation 2 consoles have the modules listed in `rom0:/EELOADCNF/IOPBTCONF` loaded when the update is loaded by `EELOAD`.
  - Not rebooting the IOP may offer non-uniform behavior across all consoles, as modules like `NCDVDMAN` are crippled.
  - Hence it is better to reboot the IOP with no argument (empty string), to load the default IOP kernel modules instead.
    ```c
    // Reboot IOP
    while(!SifIopReset("", 0)){};
    while(!SifIopSync()){};
    ```
  - *(For the PSX)*: since we need some PSX-specific services, we will need `rom0:PCDVDMAN` and `rom0:PCDVDFSV` loaded, instead of the default `CDVDMAN` and `CDVDFSV`. To achieve this, the example reboots the IOP  with an `IOPRP` image that contains a custom *IOP BooT CONFiguration* (`IOPBTCONF`) file that lists `PCDVDMAN` and `PCDVDFSV`:
    ```c
    //Reboot IOP
    while(!SifIopRebootBuffer(psx_ioprp, size_psx_ioprp)){};
    while(!SifIopSync()){};
    ```
- Initialize SIFRPC again after the IOP reboot completes.
- (*For the PSX*) In InitPSX():
  - Switch the CD/DVD drive to PS2 mode.
  - "Notify" the MECHACON of a game starting with `sceCdNotifyGameStart()`, to enable the **QUIT GAME** button. This will act as a **RESET** button.
  - Switch the memory mode from 64MB to 32MB mode, with syscall 0x85 (arbitrarily named as `SetMemoryMode()`). The switch is only binding when the TLB is initialized with the `_InitTLB()` or the PSX's `ExecPS2()` syscall.
  - Invoke the `_InitTLB()` syscall to make the switch to 32MB mode binding.
    - :warning: If the stack pointer resides above the 32MB offset at the point of remapping, a TLB exception will occur.
    ```c
    SetMemoryMode(1);
    _InitTLB();
    ```
  - Initialize system paths with `osdInitSystemPaths()`, if it is desired to retrieve the system directory names from `libosdinit`.
  - Perform boot certification to unlock the CD/DVD drive. Boot certification is done with the content of `rom0:ROMVER`.
    - This may be to validate the binding between the MECHACON and the boot ROM.
    - Do not check for the result of this operation because early PlayStation 2 models do not support boot certification (only B-chassis and later support it).
    - Maybe this may not work on the DTL-H301xx consoles because they seem to have ROM v1.50, but boot certification within OSDSYS of these sets is hardcoded to boot certify with 1.10 as the version number... but we cannot quite cover all cases without additional bloat (and very few people use those sets).
    - ROMVER strings have this format: VVVVRTYYYYMMDD (i.e. 0160HC20020426 for v1.60, Asia, CEX, 2002/04/26).
    - **Note**: This is not necessary for the PSX, as the PSX's OSDSYS will always do it before booting the update.
  Code:
    ```c
    int fd;
    char romver[16], RomName[4];
    if((fd = open("rom0:ROMVER", O_RDONLY)) >= 0)
    {
        read(fd, romver, sizeof(romver));
        close(fd);
        // E.g. 0160HC = 1,60,'H','C'
        RomName[0]=(romver[0]-'0')*10 + (romver[1]-'0');
        RomName[1]=(romver[2]-'0')*10 + (romver[3]-'0');
        RomName[2]=romver[4];
        RomName[3]=romver[5];

        sceCdBootCertify(RomName);
    }
    ```
- This is optional because we are not Sony, but you can disable DVD Video playback here:
```c
   do{
       sceCdForbidDVDP(&result);
   }while(result&8);
```
- While not done in the ROM OSD, the HDD Browser update applies the full OSD kernel patch (for SCPH-10000 & SCPH-15000) here.
  - This updates the `SetOsdConfigParam`, `GetOsdConfigParam`, `SetOsdConfigParam2` and `GetOsdConfigParam2` syscalls, as well as `ExecPS2`.
  - By updating the OSD syscalls, the kernel is also marked as fully patched.
  - This is probably only done by the HDD Browser itself (rather than the system driver update) because only a new browser can use the updated syscalls correctly. Updating the syscalls would prevent the old browser in ROM from changing the language setting (hence blocking the user) if the HDD browser update cannot be booted.
  - We can do this with:
    ```c
    InitOsd();
    ```
  - Load the OSD configuration from the EEPROM, parse it and set the settings into the EE kernel with `SetOsdConfigParam` and `SetOsdConfigParam2`.
  - `SetOsdConfigParam2` is not supported by the unpatched SCPH-10000 and SCPH-15000 kernel. Invoking this will cause an exception.
  - It is planned to move the code for loading the configuration from the EEPROM and parsing it into a new library (`libosdinit`), as to how Sony probably had... but that is still a work in progress.
  - How it is currently done in FMCB:
    ```c
    int result;
    ConfigParam config;
    Config2Param config2;

    // Load and parse configuration here. config.version is set to 2, as FMCB implements the configuration mechanism from a late ROM

    SetOsdConfigParam(&config);
    GetOsdConfigParam(&config);
    //Unpatched Protokernels cannot retain values set in the version field, and don't support SetOsdConfigParam2().
    if(config.version)
        SetOsdConfigParam2(&config2, 4, 0);
    ```
- Before the video mode is initialized with the graphics library, the GCONT (RGB/YPbPr) setting must be set:
```c
SetGsVParam(config.videoOutput?1:0);
```

## Compatibility Across PS2 Models

- Do not use the board-specific IRX modules. While some old homebrew software does that, it is bad practice because this will make your software not compatible with some (usually rarer) PlayStation 2 consoles like the early models, the PSX, and perhaps the system 246.
  - All T_ modules. T-modules like TSIO2MAN are used by TESTMODE.
  - All X_ modules. X-modules like XSIO2MAN are used by the new OSDSYS program.
  - All P\* modules. P-modules like PCDVDMAN are used by the PSX's OSDSYS program.
  - All IOPRP images like `EELOADCNF` and `OSDCNF`. There is generally no need to reboot the IOP with any of these. To reboot the IOP with the default IOP kernel modules, specify an empty string.
  - While the modules in rom0 are generally present, they aren't actually documented to be available. Even if they are present on all CEX consoles (i.e. `rom0:MCMAN`), this may cause problems on non-standard consoles like the TOOL (`rom0:MCMAN` is bounded to the SDK version!).
- After a cold boot, the controller ports seem to not be immediately powered up on the DECKARD slim consoles (SCPH-75000 and later). Even if a controller is actually connected, it appears to software as being disconnected.
  - This was probably not a problem for Sony because they did not need to implement any sort of trigger/override button (i.e. hold R1 to boot LaunchELF).
  - If necessary, wait until either a pad is detected or a timeout expires.
- As of the Dragon-series MECHACON (SCPH-50000 and later), issuing any blocking command will cause a lockup if the drive is not ready (i.e. no disc inserted), as the MECHACON no longer replies until the drive is ready.
  - OSDSYS seems to avoid this by checking on the disc type first, with `sceCdDiskType()`. Further actions are only taken if a valid disc is recognized.
  - When `sceCdInit()` is called, use the non-blocking mode to initialize the drive.
  - `sceCdDiskReady`, `sceCdSync` and `sceCdTrayReq` cannot be used when no disc is inserted.
- While the browser does have options (i.e. disc speed, texture) for the PlayStation 1 driver, these settings are always erased with every IOP reboot, when EECONF is loaded. Not being able to load them is not an issue with your code!
  - EECONF opens that configuration block of the MECHACON EEPROM and zero-fills it.
  - EECONF is responsible for initializing various peripherals. It also initializes the MAC address, SPEED capabilities and manipulates the ROM version data, by collaborating with the DECKARD emulator (SCPH-75000 and later).
  - Part of EECONF's actions were previously implemented as part of FMCB v1.8's `init.irx` module, which is unnecessary.

## OSD Initialization Libraries + Example

This package contains various files that are related to the initialization of the PlayStation 2 and the launching of the DVD Player. FMCB v1.96 and later use code based on this project.

The package contains various OSD-related files:

- *main.c*: Contains the main function, which shows how the various library functions are to be used.
- *linkfile*: linkfile for this example exists mainly for the sake of the PSX example, whereby the stack pointer must be within the first 32MB during the EE RAM capacity switch.
- *psx/ioprp.img*: Contains an IOPRP image that contains a custom IOP BooT CONFiguration (IOPBTCONF) file. This is necessary to load PCDVDMAN and PCDVDFSV from the PSX's boot ROM, to deal with the PSX-specific hardware.
- *psx/scmd_add.c*   Contains add-on functions for linking up with the PCDVDFSV module. Only for the PSX.
- *common/dvdplayer.c*: Contains DVD Player management selection and booting code.
- *common/libcdvd_add.c*: Contains add-on functions for the ancient libcdvd libraries that we use.
- *common/OSDConfig.c*: Contains OSD configuration management functions.
- *common/OSDInit.c*: Contains low-level OSD configuration-management functions.
- *common/OSDHistory.c*: Contains functions for loading and updating the user's play history.
- *common/ps1.c*:  Contains functions for booting PlayStation game discs
- *common/ps2.c*:  Contains functions for booting PlayStation 2 game discs
- *common/modelname.c*: Contains functions for retrieving the PlayStation 2 console's model name (i.e. SCPH-77006).

Most of the files are based on the OSD from ROM v2.20, which has a late design.

### Notes regarding the PSX example

The PSX's OSDSYS differs from the standard PS2's, whereby it isn't a fully-function dashboard on its own. It will also perform the boot certification step, which is why it is not necessary. The PSX's EE has 64MB and its IOP has 8MB. Similar to the TOOL, the memory capacity can be limited to 32MB with the TLB.

It also has a dual-mode CD/DVD drive - by default, it starts up in writer ("Rainbow") mode. In this mode, the usual registers do not work. The PSX also has a "QUIT GAME" button, which must be enabled by "notifying" the MECHACON of the game mode with `sceCdNotifyGameStart()`.

To build the PSX example, set the PSX variable in the Makefile to 1.

### Notes regarding disc-booting

The example shows how the browser boots PlayStation and PlayStation 2 game discs. The browser does not parse SYSTEM.CNF to determine what ELF to boot, but uses SYSTEM.CNF to verify that the ID obtained from DRM matches. Your software does not need to do this (it is not even done with FMCB). However, using the same method as the browser for parsing SYSTEM.CNF is good for ensuring that exactly the same behavior as the official browser is guaranteed. There was also code that directly accessed the hardware, to check that the appropriate type of disc is inserted. Such code has been omitted, as it seems that they were meant to enforce the DRM and increase coupling with the hardware.

### Notes regarding the DVD Player

The SCPH-10000 and SCPH-15000 have no DVD Player built-in, as no DVD ROM chip is installed.  This is much like the DEX, TOOL and PSX consoles. Starting from the SCPH-75000, the DVD ROM became universal, containing the DVD players from all regions. However, this requires the code to query the console for the MagicGate region.

### Notes regarding the functions within OSDHistory

#### icon.sys

The *icon.sys* file generated usually uses `_SCE8` as its icons. `_SCE8` usually refers to an icon that is part of the browser. The *icon.sys* file for US (icon_A.sys), Japan (icon_J.sys) and China (icon_C.sys) are reproductions of the icons that are found in the browser. While each icon.sys file is assumed to be 964 bytes in length (like normal icon.sys files), the browser always writes 1776 bytes of data when writing icon.sys. The icon is written, along with the data after it. If the correct number of bytes (964) is written, the HDD Browser appears to deem the icon as invalid.

#### ICOBYSYS

 When the console is not a US/Asian, European or Japanese console, the icon (*ICOBYSYS*) is written to the memory card as `_SCE8` (along with *icon.sys*). This leaves the Chinese (SCPH-50009) and DEX consoles (typically have an 'X' for the region) as potential targets for this code, even though these consoles appear to support `_SCE8` without problems.

However, as *ICOBYSYS* belongs to Sony, it is not part of this package. If you want to have complete system, you need to provide ICOBYSYS as `icons/icobysys.icn` and change WRITE_ICOBYSYS in the Makefile to 1.

### Known limitations

- The booting of PlayStation and PlayStation 2 discs on the Chinese console (SCPH-50009) is untested.
- Remote Control management does not actually do anything (`sceCdBypassCtl` not implemented).
- `sceCdBypassCtl()` must be implemented on the IOP. CDVDMAN does a check against the IOP PRId, and acts according to the IOP revision. Revisions before 2.3 will involve an S-command, while later revisions will involve other registers.
- The actual purpose of *OSDConfigGetRcEnabled* & *OSDConfigSetRcEnabled* is not clear. In ROM v2.20, it seems to have a relationship to a menu that is labeled "Console" and has the text "Remote Control,Off,On". However, noone find the necessary conditions for accessing this menu. In the HDD Browser, the "Remote Control,Off,On" menu text exists, but does not seem to be used. On sp193's SCPH-77006, this setting is disabled, although the other two remote control-related settings are enabled.
- Valid values for the timezone setting are currently unknown. Likely `0x00-0x7F` are valid values, but what do they really represent?
- Between the various OSD versions, Sony seemed to have changed the bit pattern to test for, when checking for runtime errors by the hardware. In ROM v1.xx, the browser's code checks against *0x09*. In the HDD Browser, the browser's code generally checks against *0x81*, while it checks against *0x09* while writing the EEPROM. In ROM v2.20 (SCPH-75000+), the browser's code checks against *0x81* in all cases. However, since the HDD Browser was made to work across different console models (although it was never intended to work on the SCPH-70000 and later), it should be safer to follow the HDD Browser.

## System Driver Update

The System Driver Update is set of patches for the OSDSYS of ROM v1.0x.

This package contains the source code for clones of the OSDSYS patches, meant for ROM v1.00 and v1.01. As with the OSD update files, these files were meant to be encapsulated as KELFs.

The first PlayStation 2 consoles (SCPH-10000 & SCPH-15000) had a considerably different boot ROM from the expansion-bay consoles. Other than having the earliest-possible IOP and EE kernels, their OSDSYS programs were coded rather differently and had a design flaw that prevented arguments from being passed to memory-card updates.

These are the original update files and their descriptions:

Code:

| Model | ROM | Update File | Description
|---|---|---|---|
| SCPH-10000            | 0100JC20000117  | osdsys.elf   | OSDSYS patch (patch0100).
| SCPH-10000/SCPH-15000 | 0101JC20000217  | osd110.elf   | System Driver Update + OSDSYS patch (patch0101).
| SCPH-18000            | 0120JC20001027  | osd130.elf   | System Driver Update + OSDSYS patch (patch0101). Exactly the same in content as osd110.elf.

These patches will copy a small ROM image that contains a replacement EELOAD program, into kernel memory (0x80030000). It will then patch the EE kernel to scan 0x80030000-0x80040000 for the EELOAD, instead of the boot ROM.

The System Driver Update attempts to boot the HDD-based update from the HDD unit. While the SCPH-18000 contained a modern ROM that did not have the same deficiencies as its predecessors, it is incapable of supporting the CXD9566R PCMCIA controller of the SCPH-18000. As a result, it needed extra help to boot updates from the HDD unit.

**osd110.elf** and **osd130.elf** are the main executables of the system driver update, which also contain a patch that is similar to the code within osdsys.elf, which installs a replacement EELOAD program that will replace rom0:EELOAD. This patch targets OSDSYS from ROM *v1.01J*, and does nothing for other ROM versions. As a result, its patches are also different from the ones that **osdsys.elf** applies (see below).

### EELOAD Replacement

This EELOAD replacement will wait within osd110.elf and osd130.elf for a request that boots `rom0:OSDSYS`, before loading `rom0:OSDSYS` and applying a few fixes to it:

1. Patch the memory card update dispatcher to pass arguments.
2. patch0100 only: Overwrite the Japanese message (`DVDプレーヤーが起動できませんでした。`), which says that that the DVD Player could not be started.
3. patch0100 only: Replaces update filenames and commands.

Once the OSDSYS-patching code within EELOAD is run, its effects are binding until the console is hard-reset or powered off.

## Modifications for FMCB, which this package has

FMCB sits in for the System Driver Update. To save space, sp193 made osd110.elf similar to osdsys.elf; instead of being a full system driver update, it will patch OSDSYS to boot osd130.elf. The osdsys.elf patch was also made to boot osd130.elf directly, hence allowing only one copy of the system driver update to exist.

Official path of execution:

- ROM v1.00J: osdsys.elf ⇨ osd110.elf ⇨ HDD update
- ROM v1.01J: osd110.elf ⇨ HDD update
- ROM v1.20J: osd130.elf ⇨ HDD update

New (unofficial) path of execution:

- ROM v1.00J: osdsys.elf ⇨ osd130.elf ⇨ HDD update
- ROM v1.01J: osd110.elf ⇨ osd130.elf ⇨ HDD update
- ROM v1.20J: osd130.elf ⇨ HDD update

## Notes for compilation

These patches are split into two parts: The main program and a ROM image that contains EELOAD. It is possible to build the patch without rebuilding EELOAD, by just entering "make".

The EELOAD module is a binary file, built to be loaded at 0x00082000. To build a ROM image, you need a tool (i.e. ROMIMG). If you wish to rebuild EELOAD, you need to:

1. Build EELOAD, by entering `make` from within the EELOAD folder.
2. Convert it into a binary file. You can do that with `ee-objdump`.
3. Generate a new ROM image named `EELOAD.img`, that contains EELOAD.
4. Rebuild the patch.

## Other Trivia

- Within the comments of the ROM images, the EELOAD images are known as `eeload.rom`.
- Their original paths were as follows:
  - osdsys.elf: horikawa@phoenix/ee/src/kernel/patch0100/kpatch
  - osd110.elf: horikawa@phoenix/ee/src/kernel/patch0101/kpatch

## MBR Program

This package contains the source code for the homebrew MBR program, which aims to function similarly to the Sony MBR program, which is used for booting HDDOSD installations.

It supports the same integrity checks as the Sony MBR program, and boots one of the following KELF targets:

- pfs0:/osd/osdmain.elf
- pfs0:/osd/hosdsys.elf
- pfs0:/osd100/hosdsys.elf

If the HDD has been deemed to have filesystem corruption, FSCK in one of these locations will be launched, in this order:

- pfs0:/fsck/fsck.elf
- pfs0:/fsck/fsck.elf
- pfs0:/fsck100/fsck.elf

Failing to do so, it'll fall back to the OSD.

### MBR Structure

This MBR has split into two parts: The main program and an embedded EELOAD module, which is used to load the HDDOSD.

The embedded EELOAD module is hardcoded in the MBR program to expect the EELOAD module to run at 0x00084000.

Unlike rom0:EELOAD and various other EELOAD programs, the SDK used to build the MBR has the alarm functions patch (which resides at 0x00082000).

The MBR program itself must run from 0x00100000 when decrypted by rom0:HDDLOAD or by a Sony OSD update.

Both programs are headerless, binary blobs of code.

*Additional note:* this MBR program is not a drop-in replacement for the PSBBN's MBR program. That one has more functionality.

The "MBR" is the first program that the ROM/System Driver Update will boot, when booting a HDD update.

As with any update file, the MBR program is meant to be stored within an appropriate KELF, but that will not be covered here.
