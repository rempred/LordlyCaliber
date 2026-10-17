int __osSpSetPc(unsigned int pc)
{
    if (!(*(volatile unsigned int *)0xA4040010 & 1))
        return -1;
    *(volatile unsigned int *)0xA4080000 = pc;
    return 0;
}
