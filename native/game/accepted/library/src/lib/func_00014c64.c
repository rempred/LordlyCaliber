void *func_00014c64(void *arg0, unsigned char *arg1) {
    float temp_f0_16;

    temp_f0_16 = (float) ((double) (float) *arg1 * *(double *)0x800AE4E0);
    (*(float *)((signed char *)(arg0) + (0x6C))) = temp_f0_16;
    (*(float *)((signed char *)(arg0) + (0x24))) = (float) ((*(float *)((signed char *)(arg0) + (0x70))) * temp_f0_16);
    return arg1 + 1;
}
