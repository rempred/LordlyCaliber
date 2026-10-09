int func_0007d994(int arg0) {
    int temp_a0_7;
    int temp_v1_8;
    int var_v0_10;

    temp_a0_7 = arg0 & 0xFF;
    temp_v1_8 = temp_a0_7 - 1;
    var_v0_10 = temp_v1_8;
    if (temp_v1_8 < 0) {
        var_v0_10 = temp_a0_7 + 2;
    }
    return ((int) (*(unsigned char *)((signed char *)(((var_v0_10 >> 2) + 0x80190000)) + (0x6A86))) >> ((temp_v1_8 & 3) * 2)) & 3;
}
