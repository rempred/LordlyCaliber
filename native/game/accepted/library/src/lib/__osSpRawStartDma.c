extern int __osSpDeviceBusy(void);
extern unsigned int osVirtualToPhysical(void *address);

int __osSpRawStartDma(int direction, unsigned int spAddress,
                      void *dramAddress, unsigned int size)
{
    if (__osSpDeviceBusy())
        return -1;
    *(volatile unsigned int *)0xA4040000 = spAddress;
    *(volatile unsigned int *)0xA4040004 = osVirtualToPhysical(dramAddress);
    if (direction == 0)
        *(volatile unsigned int *)0xA404000C = size - 1;
    else
        *(volatile unsigned int *)0xA4040008 = size - 1;
    return 0;
}
