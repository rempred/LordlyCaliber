void func_00147FB8(int m2c_unused_arg0, int arg1) {
    int *temp_a0_10;
    int temp_a1_7;
    unsigned char *temp_a1_15;

    temp_a1_7 = arg1 * 4;
    temp_a0_10 = (*(int **)((signed char *)((temp_a1_7 + 0x801F0000)) + (0xCB0)));
    temp_a1_15 = temp_a1_7 + *(int *)0x801FDA3C;
    *temp_a0_10 |= 0x04000000;
    *temp_a1_15 |= 2;
}
