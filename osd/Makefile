#Enable to write ICOBYSYS (_SCE8 icon), if the console does not belong to US, Europe, Asia or Japan.
#The icon (icobysys.icn) must be supplied.
WRITE_ICOBYSYS = 0

#Enable to enable the check against DVD Player v1.00 and v1.01.
#This will prohibit them from being booted.
PROHBIT_DVD_0100 = 0

#Enable to enable the newer sceCdReadKey checks, which are only supported by a newer CDVDMAN module.
XCDVD_READKEY = 0

#Enable to build the PSX example.
PSX = 0

EE_BIN = example.elf

IOP_OBJS = sio2man_irx.o mcman_irx.o mcserv_irx.o
ICONS = icon_sys_A.o icon_sys_J.o icon_sys_C.o
ICONS_TEMP = icon_sys_A.c icon_sys_J.c icon_sys_C.c sio2man_irx.c mcman_irx.c mcserv_irx.c
IOPRP_TEMP = ioprp.c
EE_OBJS = main.o common/libcdvd_add.o common/OSDConfig.o common/OSDInit.o common/OSDHistory.o common/dvdplayer.o common/ps1.o common/ps2.o common/modelname.o $(ICONS) $(IOP_OBJS)

EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I. -Iinclude -Ipsx
EE_LDFLAGS := -L$(PS2SDK)/ee/lib -nostartfiles -Tlinkfile
EE_LIBS := -lmc -ldebug -lpatches -lc -lkernel
EE_CFLAGS += -mno-gpopt -G0 -Os -Wall

ifeq ($(WRITE_ICOBYSYS),1)
	ICONS += icobysys_icn.o
	ICONS_TEMP += icobysys_icn.c
	EE_CFLAGS += -DWRITE_ICOBYSYS=1
endif

ifeq ($(XCDVD_READKEY),1)
	EE_CFLAGS += -DXCDVD_READKEY=1
endif

ifeq ($(PROHBIT_DVD_0100),1)
	EE_CFLAGS += -DPROHBIT_DVD_0100=1
endif

ifeq ($(PSX),1)
	EE_CFLAGS += -DPSX=1
	EE_OBJS += psx/scmd_add.o ioprp.o
	EE_LIBS += -lxcdvd -liopreboot
else
	EE_LIBS += -lcdvd
endif

%.o : %.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.o : %.s
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

%.o : %.S
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_BIN) : $(EE_OBJS)
	$(EE_CC) -T$(PS2SDK)/ee/startup/linkfile $(EE_LDFLAGS) \
		-o $(EE_BIN) $(EE_OBJS) $(PS2SDK)/ee/startup/crt0.o $(EE_LIBS)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) $(ICONS_TEMP) $(IOPRP_TEMP)

icon_sys_A.c : icons/icon_A.sys
	bin2c $< $@ icon_sys_A

icon_sys_J.c : icons/icon_J.sys
	bin2c $< $@ icon_sys_J

icon_sys_C.c : icons/icon_C.sys
	bin2c $< $@ icon_sys_C

icobysys_icn.c : icons/icobysys.icn
	bin2c $< $@ icobysys_icn

sio2man_irx.c : $(PS2SDK)/iop/irx/sio2man.irx
	bin2c $< $@ sio2man_irx

mcman_irx.c : $(PS2SDK)/iop/irx/mcman.irx
	bin2c $< $@ mcman_irx

mcserv_irx.c : $(PS2SDK)/iop/irx/mcserv.irx
	bin2c $< $@ mcserv_irx

ioprp.c : psx/ioprp.img
	bin2c $< $@ psx_ioprp

include $(PS2SDK)/Defs.make
