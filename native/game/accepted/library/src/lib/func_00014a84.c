unsigned char *func_00014a84(void *state, unsigned char *cursor)
{
    *(unsigned char *)((unsigned char *)state + 0xCA) = *cursor;
    return cursor + 1;
}
