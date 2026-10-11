void func_000ecabc(int arg0) {
    int temp_a3_21;
    int temp_v0_20;
    int temp_v0_36;
    int var_a2_19;
    int var_v1_8;

    var_v1_8 = arg0 + 2;
    *(int *)0x8018F568 = 0;
    *(signed char *)0x8018F56C = 1;
    *(signed char *)0x8018F56D = 1;
    if (var_v1_8 < 0) {
        var_v1_8 = arg0 + 5;
    }
    var_a2_19 = arg0 + 3;
    temp_v0_20 = var_v1_8 >> 2;
    temp_a3_21 = arg0 * 2;
    (*(signed char *)((signed char *)((temp_v0_20 + 0x80190000)) + (-0xAE0))) = (signed char) ((*(unsigned char *)((signed char *)((temp_v0_20 + 0x80190000)) + (-0xAE0))) | (1 << ((temp_a3_21 + 5) & 7)));
    if (var_a2_19 < 0) {
        var_a2_19 = arg0 + 6;
    }
    temp_v0_36 = var_a2_19 >> 2;
    (*(signed char *)((signed char *)((temp_v0_36 + 0x80190000)) + (-0xAE0))) = (signed char) ((*(unsigned char *)((signed char *)((temp_v0_36 + 0x80190000)) + (-0xAE0))) | (1 << ((temp_a3_21 + 6) & 7)));
}
