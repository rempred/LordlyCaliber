int func_00130794(void) {
    int var_a0_13;
    unsigned char temp_v1_8;

    temp_v1_8 = *(unsigned char *)0x8018F481;
    var_a0_13 = (temp_v1_8 == 0x3F) | (temp_v1_8 == 0x41);
    if (((temp_v1_8 == 0x40) | (temp_v1_8 == 0x42)) != 0) {
        var_a0_13 = 1;
    }
    return var_a0_13;
}
