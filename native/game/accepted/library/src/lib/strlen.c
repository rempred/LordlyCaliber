int strlen(signed char *arg0) {
    signed char *var_v1_9;

    var_v1_9 = arg0;
    if (*arg0 != 0) {
        do {
            var_v1_9 += 1;
        } while (*var_v1_9 != 0);
    }
    return var_v1_9 - arg0;
}
