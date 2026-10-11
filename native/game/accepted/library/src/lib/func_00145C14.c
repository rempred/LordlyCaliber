void func_00145C14(unsigned char *arg0, unsigned char *arg1, int arg2) {
    int var_a2_10;
    unsigned char *var_a0_0;
    unsigned char *var_a1_0;
    unsigned char temp_v0_14;

    var_a0_0 = arg0;
    var_a1_0 = arg1;
    var_a2_10 = arg2 - (arg2 / 2);
    if (var_a2_10 > 0) {
        do {
            temp_v0_14 = *var_a0_0;
            var_a0_0 += 1;
            var_a2_10 -= 1;
            *var_a1_0 = temp_v0_14;
            var_a1_0 += 1;
        } while (var_a2_10 > 0);
    }
}
