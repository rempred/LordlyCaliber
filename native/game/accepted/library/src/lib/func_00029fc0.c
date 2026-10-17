int func_00029fc0(void)
{
    unsigned int status = *(volatile unsigned int *)0xA450000C;
    if (status & 0x80000000)
        return 1;
    return 0;
}
