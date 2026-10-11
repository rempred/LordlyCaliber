unsigned char *func_00014ca0(void *state, unsigned char *cursor)
{
    *(unsigned char *)((unsigned char *)state + 0xD4) = *cursor;
    return cursor + 1;
}
