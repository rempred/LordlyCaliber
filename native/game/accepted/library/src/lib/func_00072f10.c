void func_00072f10(unsigned short arg0) {
    unsigned short temp_v0_9;
    void *temp_a1_8;

    temp_a1_8 = *(void **)0x801951AC;
    temp_v0_9 = (*(unsigned short *)((signed char *)(temp_a1_8) + (0x1A)));
    (*(unsigned short *)((signed char *)(temp_a1_8) + (0x1A))) = (unsigned short) (temp_v0_9 + 1);
    (*(unsigned short *)((signed char *)(((temp_v0_9 * 2) + temp_a1_8)) + (6))) = (unsigned short) (*(unsigned short *)((signed char *)(temp_a1_8) + (0)));
    (*(unsigned short *)((signed char *)(temp_a1_8) + (0))) = arg0;
}
