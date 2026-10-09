unsigned char *strcat(unsigned char *arg0, unsigned char *arg1) {
    unsigned char *var_a1_0;
    unsigned char *var_v1_9;
    unsigned char var_v0_16;

    var_a1_0 = arg1;
    var_v1_9 = arg0;
    if (*arg0 != 0) {
        do {
            var_v1_9 += 1;
        } while (*var_v1_9 != 0);
    }
    var_v0_16 = *var_a1_0;
    if (var_v0_16 != 0) {
        do {
            var_a1_0 += 1;
            *var_v1_9 = var_v0_16;
            var_v0_16 = *var_a1_0;
            var_v1_9 += 1;
        } while (var_v0_16 != 0);
    }
    *var_v1_9 = 0;
    return arg0;
}
