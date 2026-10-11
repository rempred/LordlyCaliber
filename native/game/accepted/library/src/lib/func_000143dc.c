void *func_000143dc(void *arg0, unsigned char *arg1) {
    unsigned char value;

    value = *arg1;
    arg1 += 1;
    (*(unsigned char *)((signed char *)(arg0) + 0xB8)) = value;
    if (value != 0) {
        (*(float *)((signed char *)(arg0) + 0x50)) = (*(float *)((signed char *)(arg0) + 0x2C));
    }
    return arg1;
}
