/*
	Filename:	EELOAD
	Description:	EE executable Loader (OSDSYS update). Largely the same as EELOAD from ROM v1.01, but has a patching mechanism.
	Date:		2013/04/25
	Arguments:
			argv[0]=<filename of ELF to load> or <"moduleload">
			(Or if "moduleload" was specified as the first argument):
			argv[1...n]= commands, where:
					"-m <module to load>"			-> Loads a regular IOP module.
					"-k <Encrypted module to load>"		-> Loads an encrypted IOP module.
					"-x <Encrypted program to load>"	-> Loads and executes an encrypted EE program.

			(Strings remaining in the argv[] array are arguments that are to be passed to the loaded program)

	Other notes:	If no arguments are specified, it loads rom0:OSDSYS.
*/

#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <string.h>
#include <ee_regs.h>
#include <dma_registers.h>

//0x00083e80
#define NUM_BOOT_PATHS	1
static char *DefaultBootPaths[NUM_BOOT_PATHS+1]={"rom0:OSDSYS", NULL};

//0x00083e88
static t_ExecData ElfData;

//0x00083ea0 - the patch that solves the problem with the browser being unable to pass arguments to the update.
//The same as the v1.01 OSDSYS patch, except for the addresses.
static const unsigned int OSDSYS_patch[]={
	0x18a0000a,	//blez a1, +11
	0x3c0b004f,	//lui t3, $004f
	0x25670100,	//addiu a3, t3, $0100
	0x00a0402d,	//daddu t0, a1, zero
	0x8cc20000,	//lw v0, $0000(a2)
	0x2508ffff,	//addiu t0, t0, $ffff
	0x24c60004,	//addiu a2, a2, $0004
	0xace20000,	//sw v0, $0000(a3)
	0x00000000,	//nop
	0x1500fffa,	//bne t0, zero, -5
	0x24e70004,	//addiu a3, a3, $0004
	0x3c0a0029,	//lui t2, $0029
	0x8d4ad280,	//lw t2, $d280(t2)
	0x24080001,	//addiu t0, zero, $0001
	0x010a102a,	//slt v0, t0, t2
	0x10400014,	//beq v0, zero, +21
	0x28a20010,	//slti v0, a1, $0010
	0x10400012,	//beq v0, zero, +19
	0x3c02004f,	//lui v0, $004f
	0x00051880,	//sll v1, a1, 2
	0x24420100,	//addiu v0, v0, $0100
	0x3c070028,	//lui a3, $0028
	0x00624821,	//addu t1, v1, v0
	0x34e7d288,	//ori a3, a3, $d288
	0x8ce30000,	//lw v1, $0000(a3)
	0x00000000,	//nop
	0x25080001,	//addiu t0, t0, $0001
	0x24e70004,	//addiu a3, a3, $0004
	0x24a50001,	//addiu a1, a1, $0001
	0xad230000,	//sw v1, $0000(t1)
	0x010a102a,	//slt v0, t0, t2
	0x10400004,	//beq v0, zero, +5
	0x25290004,	//addiu t1, t1, $0004
	0x28a20010,	//slti v0, a1, $0010
	0x5440fff7,	//bnel v0, zero, -8
	0x8ce30000,	//lw v1, $0000(a3)
	0x24030006,	//addiu v1, zero, $0006
	0x0080202d,	//daddu a0, a0, zero
	0x00a0282d,	//daddu a1, a1, zero
	0x25660100,	//addiu a2, t3, $0100
	0x00c0302d,	//daddu a2, a2, zero
	0x0000000c,	//syscall (00000)
	0x03e00008,	//jr ra
	0x00000000	//nop
};

/* 0x000820e8*/
static void PatchOSDSYS(void){
	volatile unsigned int *ptr;
	unsigned int i, size;

	FlushCache(0);

	//Copy the patch.
	strcpy((char*)0x0028b9b0, "DVDプレーヤーが起動できませんでした。");	//Don't know why they need to patch this as the original message is exactly the same. D:
	//Originally, these lines made the OSD use osd110.elf instead. But since FMCB's v1.10 update only causes the v1.01 OSD to use osd130.elf, do the same thing here. The v1.20 update is the first update that doesn't require patching.
	strcpy((char*)0x0028b770, "/BIEXEC-SYSTEM/osd130.elf");
	strcpy((char*)0x0028b790, "-x mc%d:/BIEXEC-SYSTEM/osd130.elf");
	strcpy((char*)0x0028b8a0, "osd130.elf");

	*(volatile unsigned int*)0x00204ad0=((*(volatile unsigned int*)0x00204ad0)&0xFC000000)|0x0013c000;	//jal 0x004f0000

	size=sizeof(OSDSYS_patch);	//It did a calculation on the size of the patch by subtracting the end from the start.

	for(i=0,ptr=(volatile unsigned int*)0x004f0000; i<size; ptr++,i+=4){
		*ptr=OSDSYS_patch[i/4];
	}

	FlushCache(0);
	FlushCache(2);
}

/* 0x000821d0 */
static void SyncSIF0(void){
	int i;

	//If SIF0 has incoming data, initialize SIF0 and acknowledge incoming data.
	if(*DMA_REG_STAT&0x20){
		SifSetDChain();	//Re-initialize SIF0 (IOP -> EE)
		*DMA_REG_STAT=0x20;
		while(*R_EE_SBUS_REG40&0x3000){};

		for(i=0x1000; i>0; i--){};
	}
}

/* 0x00082258 */
static void SyncIOP(void){
	while((*R_EE_SBUS_SMFLAG&SIF_STAT_BOOTEND)==0){};
	*R_EE_SBUS_SMFLAG=SIF_STAT_BOOTEND;
}

/* 0x00082298 */
static void ResetIOP(void){
	SifIopReset("", 0);
	while(!SifIopIsAlive()){};

	SyncIOP();
}

/* 0x000822c8 */
static void AckSIF0(void){
	if(*R_EE_SBUS_REG40&0x20){
		DI();
		ee_kmode_enter();

		*(volatile unsigned int*)0xBD000040=0x20;

		ee_kmode_exit();
		EI();
	}

	if(*DMA_REG_STAT&0x20)
		*DMA_REG_STAT=0x20;
}

/* 0x00082368
	Returns NULL if the argument doesn't contain the specified switch, otherwise, returns the first character after the switch.
*/
static const char *IsSwitchCheck(const char *SwitchString, const char *cmd){
	while(*SwitchString!='\0'){
		if(*SwitchString!=*cmd) return NULL;
		SwitchString++;
		cmd++;
	}

	return cmd;
}

/* 0x000823a8 */
static void ExecExecutable(int argc, char *argv[]){
	FlushCache(0);
	SifExitRpc();	//Original had SifExitCmd();

	ExecPS2((void *)ElfData.epc, (void *)ElfData.gp, argc, argv);
}

/* 0x00082400 */
static void BootError(const char *path){
	char *argv[2];

	SifExitRpc();	//Original had SifExitCmd();

	argv[0]="BootError";
	argv[1]=(char*)path;

	ExecOSD(2, argv);
}

/* 0x00082440 */
int main(int argc, char *argv[]){
	const char *CommandString;
	int i;

	if(argc>=2){
		argv++;
		argc--;
		SyncSIF0();
		ResetIOP();
		AckSIF0();

		SifInitRpc(0);

		/* 0x000824b8 */
		if(IsSwitchCheck("moduleload", argv[0])!=NULL){
			argc--;
			argv++;

			while(argc>0){
				if((CommandString=IsSwitchCheck("-m ", argv[0]))!=NULL){
					SifLoadModule(CommandString, 0, NULL);
				}
				else if((CommandString=IsSwitchCheck("-k ", argv[0]))!=NULL){
					SifLoadModuleEncrypted(CommandString, 0, NULL);
				}
				else if((CommandString=IsSwitchCheck("-x ", argv[0]))!=NULL){
					FlushCache(0);
					if(SifLoadElfEncrypted(CommandString, &ElfData)<0){
						BootError(CommandString);
					}

					argv[0]=(char*)CommandString;
					ExecExecutable(argc, argv);
				}
				else break;

				argc--;
				argv++;
			}
		}

		FlushCache(0);
		if(SifLoadElf(argv[0], &ElfData)<0){
			BootError(argv[0]);
		}

		if(IsSwitchCheck("rom0:OSDSYS", argv[0])!=NULL){
			PatchOSDSYS();
		}

		ExecExecutable(argc, argv);
	}

	SyncIOP();
	SifInitRpc(0);
	FlushCache(0);
	for(i=0; i<NUM_BOOT_PATHS; i++){
		if(SifLoadElf(DefaultBootPaths[i], &ElfData) >= 0)
			break;
	}

	PatchOSDSYS();
	ExecExecutable(1, &DefaultBootPaths[i]);

	return 0;
}
