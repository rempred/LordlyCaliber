int func_0020C034(void *object)
{
    if (object == 0) {
        return 0;
    }

    return (*(unsigned int *)((unsigned char *)object + 0x40) >> 10) & 1;
}
