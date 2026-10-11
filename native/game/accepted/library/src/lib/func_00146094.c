int func_00146094(void *arg0) {
    int masked_value;
    int result;

    result = 0;
    if ((*(int *)((signed char *)(arg0) + 0x70)) == 1) {
        masked_value = (*(int *)((signed char *)(arg0) + 0)) & 0x40;
        result = masked_value == 0;
    }
    return result;
}
