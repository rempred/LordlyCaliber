int func_00210464(int arg0, int arg1) {
    int var_a0_0;
    int var_v1_9;

    var_a0_0 = arg0;
    var_v1_9 = 0;
    if (var_a0_0 >= arg1) {
        do {
            var_v1_9 += var_a0_0;
            var_a0_0 -= 1;
        } while (var_a0_0 >= arg1);
    }
    return var_v1_9;
}
