int func_0020BFF8(void *arg0)
{
    if (arg0 == 0) {
        return 0;
    }

    return (*(unsigned int *)((unsigned char *)arg0 + 0x40) >> 8) & 1;
}
