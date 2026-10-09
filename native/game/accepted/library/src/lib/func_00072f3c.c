void func_00072f3c(void) {
    unsigned short temp_v1_10;
    void *temp_v0_8;

    temp_v0_8 = *(void **)0x801951AC;
    temp_v1_10 = (*(unsigned short *)((signed char *)(temp_v0_8) + (0x1A))) - 1;
    (*(unsigned short *)((signed char *)(temp_v0_8) + (0x1A))) = temp_v1_10;
    (*(unsigned short *)((signed char *)(temp_v0_8) + (0))) = (unsigned short) (*(unsigned short *)((signed char *)((((temp_v1_10 & 0xFFFF) * 2) + temp_v0_8)) + (6)));
}
