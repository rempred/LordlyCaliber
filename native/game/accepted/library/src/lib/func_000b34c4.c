void func_000b34c4(float *arg0, float *arg1, int arg2) {
    float temp_f10_19;
    float temp_f6_29;

    temp_f10_19 = (float) (arg2 / 6);
    temp_f6_29 = (float) (5 - (arg2 % 6));
    *arg0 = (10.0f - (temp_f10_19 * 36.0f)) + (temp_f6_29 * 15.0f) + 0.5f;
    *arg1 = ((temp_f6_29 * 25.0f) + -5.0f + (temp_f10_19 * 10.0f) + 0.5f) - 72.0f;
}
