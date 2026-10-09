void func_001F10F0(void *arg0, void *arg1) {
    (*(float *)((signed char *)(arg1) + (0x10))) = (float) ((*(float *)((signed char *)(arg1) + (0x10))) + (*(float *)((signed char *)(arg1) + (0x18))));
    (*(float *)((signed char *)(arg1) + (0x14))) = (float) ((*(float *)((signed char *)(arg1) + (0x14))) + (*(float *)((signed char *)(arg1) + (0x1C))));
    (*(signed short *)((signed char *)(arg0) + (0x1C))) = (signed short) (int) ((*(float *)((signed char *)(arg1) + (8))) + (*(float *)((signed char *)(arg1) + (0x10))));
    (*(signed short *)((signed char *)(arg0) + (0x20))) = (signed short) (int) ((*(float *)((signed char *)(arg1) + (0xC))) + (*(float *)((signed char *)(arg1) + (0x14))));
}
