EE_BIN = osdsys.elf

EE_OBJS = osdsys.o EELOAD_img.o

include $(PS2SDK)/Defs.make
include ../Rules.patch.make

clean:
	rm -f EELOAD_img.c

EELOAD_img.o:
	bin2c EELOAD.img EELOAD_img.c EELOAD_img
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c EELOAD_img.c -o EELOAD_img.o
