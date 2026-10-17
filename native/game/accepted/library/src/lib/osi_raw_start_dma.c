extern void osWritebackDCache(void *address, int size);
extern void osInvalDCache(void *address, int size);
extern unsigned int osVirtualToPhysical(void *address);

int osi_raw_start_dma(int direction, void *address)
{
    if (*(volatile unsigned int *)0xA4800018 & 3)
        return -1;
    if (direction == 1)
        osWritebackDCache(address, 0x40);
    *(volatile unsigned int *)0xA4800000 = osVirtualToPhysical(address);
    if (direction == 0)
        *(volatile unsigned int *)0xA4800004 = 0x1FC007C0;
    else
        *(volatile unsigned int *)0xA4800010 = 0x1FC007C0;
    if (direction == 0)
        osInvalDCache(address, 0x40);
    return 0;
}
