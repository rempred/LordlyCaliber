unsigned char *func_00014b84(void *state, unsigned char *cursor)
{
    *(unsigned char *)((unsigned char *)state + 0xBC) = *cursor;
    return cursor + 1;
}
