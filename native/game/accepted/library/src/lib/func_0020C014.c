int func_0020C014(void *object)
{
    if (object == 0) {
        return 0;
    }

    return ((*(unsigned int *)((unsigned char *)object + 0x40) >> 8) ^ 1) & 1;
}
