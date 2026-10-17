int read_vi_current_field(void)
{
    unsigned int status = *(volatile unsigned int *)0xA410000C;
    if (status & 0x100)
        return 1;
    return 0;
}
