void *func_00014628(void *arg0, unsigned char *arg1) {
    unsigned char high_byte;
    unsigned char low_byte;

    high_byte = *arg1;
    arg1 += 1;
    low_byte = *arg1;
    arg1 += 1;
    (*(signed short *)((signed char *)(arg0) + 0xB4)) = 0;
    (*(signed short *)((signed char *)(arg0) + 0xB2)) = (signed short)(low_byte | (high_byte << 8));
    return arg1;
}
