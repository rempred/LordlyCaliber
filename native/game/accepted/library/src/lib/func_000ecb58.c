void func_000ecb58(int arg0) {
    int temp_a3_14;
    int temp_v0_13;
    int temp_v0_30;
    int var_a2_12;
    int var_v0_8;

    var_v0_8 = arg0 + 2;
    if (var_v0_8 < 0) {
        var_v0_8 = arg0 + 5;
    }
    var_a2_12 = arg0 + 3;
    temp_v0_13 = var_v0_8 >> 2;
    temp_a3_14 = arg0 * 2;
    (*(signed char *)((signed char *)((temp_v0_13 + 0x80190000)) + (-0xAE0))) = (signed char) ((*(unsigned char *)((signed char *)((temp_v0_13 + 0x80190000)) + (-0xAE0))) & ~(1 << ((temp_a3_14 + 5) & 7)));
    if (var_a2_12 < 0) {
        var_a2_12 = arg0 + 6;
    }
    temp_v0_30 = var_a2_12 >> 2;
    (*(signed char *)((signed char *)((temp_v0_30 + 0x80190000)) + (-0xAE0))) = (signed char) ((*(unsigned char *)((signed char *)((temp_v0_30 + 0x80190000)) + (-0xAE0))) & ~(1 << ((temp_a3_14 + 6) & 7)));
}
