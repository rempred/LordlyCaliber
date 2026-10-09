int func_0005c110(int arg0) {
    int temp_v1_11;
    int var_v1_8;

    var_v1_8 = arg0;
    if (arg0 < 0) {
        var_v1_8 = arg0 + 7;
    }
    temp_v1_11 = var_v1_8 >> 3;
    return ((int) (*(unsigned char *)((signed char *)((temp_v1_11 + 0x80190000)) + (0x7168))) >> (arg0 - (temp_v1_11 * 8))) & 1;
}
