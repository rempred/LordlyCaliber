int read_si_status_busy(void)
{
    unsigned int status = *(volatile unsigned int *)0xA4800018;
    if (status & 3)
        return 1;
    return 0;
}
