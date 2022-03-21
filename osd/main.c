#include <stdio.h>
#include <kernel.h>
#include <fileio.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <libcdvd.h>
#include <libmc.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <osd_config.h>

#include <sbv_patches.h>

#include "main.h"
#include "libcdvd_add.h"
#include "dvdplayer.h"
#include "OSDInit.h"
#include "OSDConfig.h"
#include "OSDHistory.h"
#include "ps1.h"
#include "ps2.h"
#include "modelname.h"

extern unsigned char sio2man_irx[];
extern unsigned int size_sio2man_irx;

extern unsigned char mcman_irx[];
extern unsigned int size_mcman_irx;

extern unsigned char mcserv_irx[];
extern unsigned int size_mcserv_irx;

//As our homebrew SDK can be linked against any either fileio or fileXio
extern int (*_ps2sdk_close)(int) __attribute__((section("data")));
extern int (*_ps2sdk_open)(const char*, int) __attribute__((section("data")));
extern int (*_ps2sdk_read)(int, void*, int) __attribute__((section("data")));

void CleanUp(void)
{	//This is called from DVDPlayerBoot(). Deinitialize all RPCs here.
	sceCdInit(SCECdEXIT);
}

static void AlarmCallback(s32 alarm_id, u16 time, void *common)
{
	iWakeupThread((int)common);
}

int main(int argc, char *argv[])
{
	int fd, OldDiscType, DiscType, ValidDiscInserted, result;
	u32 stat;
	char romver[16], RomName[4];

	//Initialize SIFCMD & SIFRPC
	SifInitRpc(0);

	//Reboot IOP
	//For this demo, this is disabled so that printf() can work through RDB/ps2link.
/*	while(!SifIopReset("", 0)){};
	while(!SifIopSync()){};	*/

	//Initialize SIFCMD & SIFRPC
	SifInitRpc(0);
	SifInitIopHeap();
	SifLoadFileInit();
	fioInit();

	//The old IOP kernel has no support for LoadModuleBuffer. Apply the patch to enable it.
	sbv_patch_enable_lmb();
	/*	Load the SIO2 modules. You may choose to use the ones from ROM,
		but they may not be supported by all PlayStation 2 variants. */
	SifExecModuleBuffer(sio2man_irx, size_sio2man_irx, 0, NULL, NULL);
	SifExecModuleBuffer(mcman_irx, size_mcman_irx, 0, NULL, NULL);
	SifExecModuleBuffer(mcserv_irx, size_mcserv_irx, 0, NULL, NULL);
	//Initialize libmc.
	mcInit(MC_TYPE_XMC);

	//Load ADDDRV. The OSD has it listed in rom0:OSDCNF/IOPBTCONF, but it is otherwise not loaded automatically.
	SifLoadModule("rom0:ADDDRV", 0, NULL);

	//Initialize libcdvd & supplement functions (which are not part of the ancient libcdvd library we use).
	sceCdInit(SCECdINoD);
	cdInitAdd();

	//Initialize system paths.
	OSDInitSystemPaths();

	//Perform boot certification to enable the CD/DVD drive.
	if((fd = _ps2sdk_open("rom0:ROMVER", O_RDONLY)) >= 0)
	{
		_ps2sdk_read(fd, romver, sizeof(romver));
		_ps2sdk_close(fd);

		// e.g. 0160HC = 1,60,'H','C'
		RomName[0]=(romver[0]-'0')*10 + (romver[1]-'0');
		RomName[1]=(romver[2]-'0')*10 + (romver[3]-'0');
		RomName[2]=romver[4];
		RomName[3]=romver[5];

		//Do not check for success/failure. Early consoles do not support (and do not require) boot-certification.
		sceCdAltBootCertify(RomName);
	}

	//This disables DVD Video Disc playback. This functionality is restored by loading a DVD Player KELF.
	/*	Hmm. What should the check for stat be? In v1.xx, it seems to be a check against 0x08. In v2.20, it checks against 0x80.
		The HDD Browser does not call this function, but I guess it would check against 0x08. */
/*	do{
		sceCdForbidDVDP(&stat);
	}while(stat & 0x08); */

	//Apply kernel updates for applicable kernels.
	InitOsd();

	//Initialize ROM version (must be done first).
	OSDInitROMVER();

	//Initialize model name
	ModelNameInit();

	//Initialize PlayStation Driver (PS1DRV)
	PS1DRVInit();

	//Initialize ROM DVD player.
	//It is normal for this to fail on consoles that have no DVD ROM chip (i.e. DEX or the SCPH-10000/SCPH-15000).
	DVDPlayerInit();

	//Load OSD configuration
	if(OSDConfigLoad() != 0)
	{	//OSD configuration not initialized. Defaults loaded.
		printf("OSD Configuration not initialized. Defaults loaded.\n");
	}
	
	//Applies OSD configuration (saves settings into the EE kernel)
	OSDConfigApply();

	/*	Try to enable the remote control, if it is enabled.
		Indicate no hardware support for it, if it cannot be enabled. */
	do {
		result = sceCdAltRcBypassCtl(OSDConfigGetRcGameFunction() ^ 1, &stat);
		if(stat & 0x100)
		{	//Not supported by the PlayStation 2.
			//Note: it does not seem like the browser updates the NVRAM here to change this status.
			OSDConfigSetRcEnabled(0);
			OSDConfigSetRcSupported(0);
			break;
		}
	} while((stat & 0x80) || (result == 0));
	
	//Remember to set the video output option (RGB or Y Cb/Pb Cr/Pr) accordingly, before SetGsCrt() is called.
	SetGsVParam(OSDConfigGetVideoOutput() == VIDEO_OUTPUT_RGB ? 0 : 1);

	printf(	"SIDIF Mode:\t%u\n"
		"Screen type:\t%u\n"
		"Video mode:\t%u\n"
		"Language:\t%u\n"
		"PS1DRV config:\t0x%02x\n"
		"Timezone offset:\t%u\n"
		"Daylight savings:\t%u\n"
		"Time format:\t%u\n"
		"Date format:\t%u\n",	OSDConfigGetSPDIF(),
					OSDConfigGetScreenType(),
					OSDConfigGetVideoOutput(),
					OSDConfigGetLanguage(),
					OSDConfigGetPSConfig(),
					OSDConfigGetTimezoneOffset(),
					OSDConfigGetDaylightSaving(),
					OSDConfigGetTimeFormat(),
					OSDConfigGetDateFormat());

	/*	If required, make any changes with the getter/setter functions in OSDConfig.h, before calling OSDConfigSave(1).
	Example:
		OSDConfigSetScreenType(TV_SCREEN_169);
		OSDConfigSave(0);
		OSDConfigApply(); */

	printf(	"\nModel:\t\t%s\n"
		"PlayStation Driver:\t%s\n"
		"DVD Player:\t%s\n",	ModelNameGet(),
					PS1DRVGetVersion(),
					DVDPlayerGetVersion());


	/*	Update Play History (if required)
		This is now more of a FYI, since the DVD and PS/PS2 game disc-booting code will do it for you.
		Provide the boot filename (without the ";1" suffix) for PlayStation/PlayStation 2 games. */
//	UpdatePlayHistory("SLPM_550.52");

	/*	If the user chose to have diagnosis enabled, it can be enabled/disabled at any time.
		The browser does this in the background, but this is put here to show how it is done.

		Hmm. What should the check for stat be? In v1.xx, it seems to be a check against 0x08. In v2.20, it checks against 0x80.
		The HDD browser checks for 0x08.
		But because we are targeting all consoles, it would be probably safer to follow the HDD Browser. */
	printf("Enabling Diagnosis...\n");
	do {	//0 = enable, 1 = disable.
		result = sceCdAutoAdjustCtrl(0, &stat);
	} while((stat & 0x08) || (result == 0));

	//For this demo, wait for a valid disc to be inserted.
	printf("Waiting for disc to be inserted...\n");

	ValidDiscInserted = 0;
	OldDiscType = -1;
	while(!ValidDiscInserted)
	{
		DiscType = sceCdGetDiskType();
		if(DiscType != OldDiscType)
		{
			printf("New Disc:\t");
			OldDiscType = DiscType;

			switch(DiscType)
			{
				case SCECdNODISC:
					printf("No Disc\n");
					break;

				case SCECdDETCT:
				case SCECdDETCTCD:
				case SCECdDETCTDVDS:
				case SCECdDETCTDVDD:
					printf("Reading Disc...\n");
					break;

				case SCECdPSCD:
				case SCECdPSCDDA:
					printf("PlayStation\n");
					ValidDiscInserted = 1;
					break;

				case SCECdPS2CD:
				case SCECdPS2CDDA:
				case SCECdPS2DVD:
					printf("PlayStation 2\n");
					ValidDiscInserted = 1;
					break;

				case SCECdCDDA:
					printf("Audio Disc (not supported by this demo)\n");
					break;

				case SCECdDVDV:
					printf("DVD Video\n");
					ValidDiscInserted = 1;
					break;
				default:
					printf("Unknown\n");
			}
		}

		//Avoid spamming the IOP with sceCdGetDiskType(), or there may be a deadlock.
		//The NTSC/PAL H-sync is approximately 16kHz. Hence approximately 16 ticks will pass every millisecond.
		SetAlarm(1000 * 16, &AlarmCallback, (void*)GetThreadId());
		SleepThread();
	}

	//Now that a valid disc is inserted, do something.
	//CleanUp() will be called, to deinitialize RPCs. SIFRPC will be deinitialized by the respective disc-handlers.
	switch(DiscType)
	{
		case SCECdPSCD:
		case SCECdPSCDDA:
			//Boot PlayStation disc
			PS1DRVBoot();
			break;

		case SCECdPS2CD:
		case SCECdPS2CDDA:
		case SCECdPS2DVD:
			//Boot PlayStation 2 disc
			PS2DiscBoot();
			break;

		case SCECdDVDV:
			/*	If the user chose to disable the DVD Player progressive scan setting,
				it is disabled here because Sony probably wanted the setting to only bind if the user played a DVD.
				The original did the updating of the EEPROM in the background, but I want to keep this demo simple.
				The browser only allowed this setting to be disabled, by only showing the menu option for it if it was enabled by the DVD Player. */
			/*	OSDConfigSetDVDPProgressive(0);
				OSDConfigApply(); */

			/*	Boot DVD Player. If one is stored on the memory card and is newer, it is booted instead of the one from ROM.
				Play history is automatically updated. */
			DVDPlayerBoot();
			break;
	}

	/*	If execution reaches here, SIFRPC has been initialized. You can choose to exit, or do something else.
		But if you do something else that requires SIFRPC, remember to re-initialize SIFRPC first. */

	return 0;
}
