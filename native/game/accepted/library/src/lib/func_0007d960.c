int func_0007d960(int arg0) {
    int temp_a0_7;
    int temp_v1_8;
    int var_v0_10;

    temp_a0_7 = arg0 & 0xFF;
    temp_v1_8 = temp_a0_7 - 1;
    var_v0_10 = temp_v1_8;
    if (temp_v1_8 < 0) {
        var_v0_10 = temp_a0_7 + 6;
    }
    return ((int) (*(unsigned char *)((signed char *)(((var_v0_10 >> 3) + 0x80190000)) + (0x6A81))) >> (temp_v1_8 & 7)) & 1;
}
