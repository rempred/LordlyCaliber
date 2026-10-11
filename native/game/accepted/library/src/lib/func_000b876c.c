void func_000b876c(signed short arg0, signed short arg1, signed char arg2) {
    void *temp_v0_8;

    temp_v0_8 = *(void **)0x80196AF8;
    (*(signed char *)((signed char *)(temp_v0_8) + (0x180))) = arg2;
    if (arg2 & 0xFF) {
        (*(signed short *)((signed char *)(temp_v0_8) + (0x17C))) = arg0;
        (*(signed short *)((signed char *)(temp_v0_8) + (0x17E))) = arg1;
    }
}
