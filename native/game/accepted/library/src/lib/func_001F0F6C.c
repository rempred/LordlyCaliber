void func_001F0F6C(int arg0) {
    int temp_v1_9;
    void *temp_v0_8;

    temp_v0_8 = *(void **)0x801CE8BC;
    temp_v1_9 = (*(int *)((signed char *)(temp_v0_8) + (0x56C0)));
    (*(int *)((signed char *)(temp_v0_8) + (0x56C0))) = (int) (temp_v1_9 + 1);
    (*(int *)((signed char *)(((temp_v1_9 * 4) + temp_v0_8)) + (0x52C0))) = arg0;
}
