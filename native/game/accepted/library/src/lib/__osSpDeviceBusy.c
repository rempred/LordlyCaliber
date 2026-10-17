int __osSpDeviceBusy(void)
{
    unsigned int status = *(volatile unsigned int *)0xA4040010;
    if (status & 0x1C)
        return 1;
    return 0;
}
