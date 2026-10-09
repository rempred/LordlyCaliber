unsigned char *func_00014988(void *state, unsigned char *cursor)
{
    unsigned char value = *cursor;

    *(signed char *)((unsigned char *)state + 0xD2) = 0;
    *(unsigned char *)((unsigned char *)state + 0xD3) = value;
    return cursor + 1;
}
