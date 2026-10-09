void func_002A91E4(float arg0, float arg1, float arg2, float arg3, int arg4) {
    float temp_f2_15;
    void *temp_v0_9;

    temp_v0_9 = (*(void **)((signed char *)(*(void **)0x8022A974) + (0x19F0)));
    temp_f2_15 = (float) arg4;
    (*(signed short *)((signed char *)(temp_v0_9) + (0x48))) = (signed short) arg4;
    (*(float *)((signed char *)(temp_v0_9) + (0xC))) = (float) ((arg0 - (*(float *)((signed char *)(temp_v0_9) + (0x30)))) / temp_f2_15);
    (*(float *)((signed char *)(temp_v0_9) + (0x10))) = (float) ((arg1 - (*(float *)((signed char *)(temp_v0_9) + (0x34)))) / temp_f2_15);
    (*(float *)((signed char *)(temp_v0_9) + (0))) = (float) ((arg2 - (*(float *)((signed char *)(temp_v0_9) + (0x24)))) / temp_f2_15);
    (*(float *)((signed char *)(temp_v0_9) + (4))) = (float) ((arg3 - (*(float *)((signed char *)(temp_v0_9) + (0x28)))) / temp_f2_15);
}
