extern int read_si_status_busy(void);

int func_0002ac80(unsigned int address, unsigned int value)
{
    if (read_si_status_busy())
        return -1;
    *(volatile unsigned int *)(address | 0xA0000000) = value;
    return 0;
}
