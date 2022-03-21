OSDSYS patch for ROM v1.0x	- 2018/01/29
--------------------------------------------

This package contains the source code for clones of the OSDSYS patches, meant for ROM v1.00 and v1.01.
As with the OSD update files, these files were meant to be encapsulated as KELFs. But I will not cover that here.

The first PlayStation 2 consoles (SCPH-10000 & SCPH-15000) had a considerably different boot ROM from the expansion-bay consoles.
Other than having the earliest-possible IOP and EE kernels, their OSDSYS programs were coded rather differently and had a design flaw that prevented arguments from being passed to memory-card updates.

These are the original update files and their descriptions:

Model                 ROM             Update File  Description
SCPH-10000            0100JC20000117  osdsys.elf   OSDSYS patch (patch0100).
SCPH-10000/SCPH-15000 0101JC20000217  osd110.elf   System Driver Update + OSDSYS patch (patch0101).
SCPH-18000            0120JC20001027  osd130.elf   System Driver Update + OSDSYS patch (patch0101). Exactly the same in content as osd110.elf.

The OSDSYS patch will copy a ROM image that contains a replacement EELOAD program, into kernel memory (0x80030000).
It will then patch the EE kernel to scan 0x80030000-0x80040000 for the EELOAD, instead of the boot ROM.

The System Driver Update attempts to boot the HDD-based update from the HDD unit.
While the SCPH-18000 contained a modern ROM that did not have the same deficiencies as its predecessors,
it is incapable of supporting the CXD9566R PCMCIA controller of the SCPH-18000. As a result, it needed extra help to boot updates from the HDD unit.

osd110.elf and osd130.elf are the system driver update main executables, which also contain a patch that is similar to the code within osdsys.elf, which install a replacement EELOAD program that will replace rom0:EELOAD.
This patch targets OSDSYS from ROM v1.01J, and does nothing for other ROM versions.
As a result, its patches are also different from the ones that osdsys.elf applies (see below).

This EELOAD replacement will wait for a request that boots rom0:OSDSYS, before loading rom0:OSDSYS and applying a few fixes to it:
	1. Patch the memory card update dispatcher to pass arguments.
	2. patch0100 only: Overwrite the Japanese message (DVDプレーヤーが起動できませんでした。), which says that that the DVD Player could not be started.
	3. patch0100 only: Replaces update filenames and commands.

Once the OSDSYS-patching code within EELOAD is run, its effects are binding until the console is hard-reset or powered off.

Modifications for FMCB, which this package has:
-----------------------------------------------
FMCB sits in for the system driver update. To save space, I made osd110.elf similar to osdsys.elf; instead of being a full system driver update, it will patch OSDSYS to boot osd130.elf.
The osdsys.elf patch was also made to boot osd130.elf directly, hence allowing only one copy of the system driver update to exist.

Official path of execution:
ROM v1.00J: osdsys.elf -> osd110.elf -> HDD update
ROM v1.01J: osd110.elf -> HDD update
ROM v1.20J: osd130.elf -> HDD update

New (unofficial) path of execution:
ROM v1.00J: osdsys.elf -> osd130.elf -> HDD update
ROM v1.01J: osd110.elf -> osd130.elf -> HDD update
ROM v1.20J: osd130.elf -> HDD update

Notes for compilation:
----------------------
These patches are split into two parts: The main program and an ROM image that contains EELOAD.
It is possible to build the patch without rebuilding EELOAD, by just entering "make".

The EELOAD module is a binary file, built to be loaded at 0x00082000. To build a ROM image, you need a tool (i.e. ROMIMG).
If you wish to rebuild EELOAD, you need to:
1. Build EELOAD, by entering "make" from within the EELOAD folder.
2. Convert it into a binary file. You can do that with ee-objdump.
3. Generate a new ROM image named 'EELOAD.img', that contains EELOAD.
4. Rebuild the patch.

Other trivia:
-------------
Within the comments of the ROM images, the EELOAD images are known as "eeload.rom".
Their original paths were as follows:
osdsys.elf: horikawa@phoenix/ee/src/kernel/patch0100/kpatch
osd110.elf: horikawa@phoenix/ee/src/kernel/patch0101/kpatch

