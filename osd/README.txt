OSD libary v1.04 - 2019/12/07
-----------------------------

The package contains various OSD-related files:
	main.c:			Contains the main function, which shows how the various library functions are to be used.
	linkfile		linkfile for this example.
				Exists mainly for the sake of the PSX example, whereby the stack pointer must be within the first 32MB during the EE RAM capacity switch.
	psx/ioprp.img:		Contains an IOPRP image that contains a custom IOP BooT CONFiguration (IOPBTCONF) file.
				This is necessary to load PCDVDMAN and PCDVDFSV from the PSX's boot ROM, to deal with the PSX-specific hardware.
	psx/scmd_add.c		Contains add-on functions for linking up with the PCDVDFSV module. Only for the PSX.
	common/dvdplayer.c:	Contains DVD Player management selection and booting code.
	common/libcdvd_add.c:	Contains add-on functions for the ancient libcdvd libraries that we use.
	common/OSDConfig.c:	Contains OSD configuration management functions.
	common/OSDInit.c:	Contains low-level OSD configuration-management functions.
	common/OSDHistory.c:	Contains functions for loading and updating the user's play history.
	common/ps1.c		Contains functions for booting PlayStation game discs
	common/ps2.c		Contains functions for booting PlayStation 2 game discs
	common/modelname.c	Contains functions for retrieving the PlayStation 2 console's model name (i.e. SCPH-77006).

	Most of files were based on the OSD from ROM v2.20, which has a late design.

Notes regarding the PSX example:
	The PSX's OSDSYS differs from the standard PS2's, whereby it isn't a fully-function dashboard on its own.
	It will also perform the boot certification step, which is why it is not necessary.
	The PSX's EE has 64MB and its IOP has 8MB. Similar to the TOOL, the memory capacity can be limited to 32MB with the TLB.
	It also has a dual-mode CD/DVD drive - by default, it starts up in writer ("Rainbow") mode. In this mode, the usual registers do not work.
	The PSX also has a "QUIT GAME" button, which must be enabled by "notifying" the MECHACON of the game mode with sceCdNotifyGameStart().

	To build the PSX example, set the PSX variable in the Makefile to 1.

Notes regarding disc-booting:
	The example shows how the browser boots PlayStation and PlayStation 2 game discs.
	The browser does not parse SYSTEM.CNF to determine what ELF to boot, but uses
	SYSTEM.CNF to verify that the ID obtained from DRM matches.

	Your software does not need to do this (it is not even done with FMCB).
	However, using the same method as the browser for parsing SYSTEM.CNF is good
	for ensuring that exactly the same behaviour as the official browser is guaranteed.

	There was also code that directly accessed the hardware, to check that the appopriate type of disc is inserted.
	Such code has been omitted, as I feel that they were meant to enforce the DRM and increases coupling with the hardware.

Notes regarding the DVD Player:
	The SCPH-10000 and SCPH-15000 have no DVD Player built in, as no DVD ROM chip is installed.
	This is much like the DEX, TOOL and PSX consoles.

	Starting from the SCPH-75000, the DVD ROM became universal, containing the DVD players from all regions.
	However, this requires the code to query the console for the MagicGate region.

Notes regarding the functions within OSDHistory:
	icon.sys:
	The icon.sys file generated usually uses "_SCE8" as its icons.
	"_SCE8" usually refers to an icon that is part of the browser.

	The icon.sys file for US (icon_A.sys), Japan (icon_J.sys) and China (icon_C.sys)
	are reproductions of the icons that are found in the browser.
	While each icon.sys file is assumed to be 964 bytes in length (like normal icon.sys files),
	the browser always writes 1776 bytes of data when writing icon.sys. The icon is written, along with the data after it.
	If the correct number of bytes (964) is written, the HDD Browser appears to deem the icon as invalid.

	ICOBYSYS:
	When the console is not a US/Asian, European or Japanese console,
	the icon (ICOBYSYS) is written to the memory card as "_SCE8" (along with icon.sys).
	This leaves the Chinese (SCPH-50009) and DEX consoles (typically have an 'X' for the region) as potential targets for this code,
	even though these consoles appear to support "_SCE8" without problems.

	However, as ICOBYSYS belongs to Sony, it is not part of this package.
	If you want to have a complete system, you need to provide ICOBYSYS as icons/icobysys.icn and
	change WRITE_ICOBYSYS in the Makefile to 1.

Known limitations:
	*Booting of PlayStation and PlayStation 2 discs on the Chinese console (SCPH-50009) is untested.
	*Remote Control management does not actually do anything (sceCdBypassCtl not implemented).
	 sceCdBypassCtl() must be implemented on the IOP. CDVDMAN does a check against the IOP PRId, and acts according to the IOP revision.
	 Revisions before 2.3 will involve a S-command, while later revisions will involve other registers.
	 The only true way would be to execute code on the IOP. An easy way would be to use XCDVDMAN+XCDVDFSV from the target PS2.
	 However, this is a board-specific function that might not exist in the XCDVDMAN module versions from all PS2s.
	*The actual purpose of OSDConfigGetRcEnabled & OSDConfigSetRcEnabled is not clear.
		I am not entirely sure what this is.
		In ROM v2.20, it seems to have a relationship to a menu that is labeled "Console" and has the text "Remote Control,Off,On".
		However, I could not find the necessary conditions for accessing this menu.
		In the HDD Browser, the "Remote Control,Off,On" menu text exists, but does not seem to be used.
		On my SCPH-77006, this setting is disabled, although the other two remote control-related settings are enabled.
	*Valid values for the timezone setting are currently unknown.
		Likely 0x00-0x7F are valid values, but what do they really represent?
	*Between the various OSD versions, Sony seemed to have changed the bit pattern to test for, when checking for runtime errors by the hardware.
		In ROM v1.xx, the browser's code checks against 0x09.
		In the HDD Browser, the browser's code generally checks against 0x81, while it checks against 0x09 while writing the EEPROM.
		In ROM v2.20 (SCPH-75000+), the browser's code checks against 0x81 in all cases.
	 However, since the HDD Browser was made to work across different console models (although it was never intended to work on the SCPH-70000 and later),
	 it should be safer to follow the HDD Browser.
