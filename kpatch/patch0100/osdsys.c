#include <kernel.h>

extern unsigned char EELOAD_img[];
extern unsigned int size_EELOAD_img;

void *_start(void)
{
    unsigned int i;
    vu32 *start;

    DI();
    ee_kmode_enter();

    for (i = 0, start = (vu32 *)0x80030000; i < size_EELOAD_img / 4; i++)
    {
        start[i] = ((vu32 *)EELOAD_img)[i];
    }

    *(vu16 *)0x80005670 = 0x8003; // Patch the start of address range of the IOPRP image containing EELOAD.
    *(vu16 *)0x80005674 = 0x8004; // Patch the end of address range of the IOPRP image containing EELOAD.

    ee_kmode_exit();
    EI();

    FlushCache(0);
    FlushCache(2);

    ExecOSD(0, NULL);

    return ((void *)0x80005670);
}
