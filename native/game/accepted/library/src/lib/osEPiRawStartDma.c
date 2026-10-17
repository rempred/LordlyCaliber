extern unsigned int D_80000308;
extern unsigned int osVirtualToPhysical(void *address);

int osEPiRawStartDma(int direction, unsigned int deviceAddress,
                    void *dramAddress, unsigned int size)
{
    while (*(volatile unsigned int *)0xA4600010 & 3) {
    }
    *(volatile unsigned int *)0xA4600000 = osVirtualToPhysical(dramAddress);
    *(volatile unsigned int *)0xA4600004 =
        (D_80000308 | deviceAddress) & 0x1FFFFFFF;
    switch (direction) {
    case 0:
        *(volatile unsigned int *)0xA460000C = size - 1;
        break;
    case 1:
        *(volatile unsigned int *)0xA4600008 = size - 1;
        break;
    default:
        return -1;
    }
    return 0;
}
