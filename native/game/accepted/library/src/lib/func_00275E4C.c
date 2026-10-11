void func_00275E4C(void *state)
{
    unsigned short *flags = (unsigned short *)((unsigned char *)state + 0x20);

    *flags &= 0xFFFE;
}
