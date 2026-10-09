unsigned char *func_00014614(void *state, unsigned char *cursor)
{
    unsigned char value = *cursor;

    *(short *)((unsigned char *)state + 0xB2) = 0;
    *(short *)((unsigned char *)state + 0xB4) = value;
    return cursor + 1;
}
