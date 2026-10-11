unsigned char func_000454e0(int arg0) {
    unsigned char var_a0_11;

    var_a0_11 = (*(unsigned char *)((signed char *)((((arg0 & 0xFFFF) << 5) + 0x80190000)) + (-0x3BEF)));
    if ((var_a0_11 & 0xFF) == 0x10) {
        var_a0_11 = *(unsigned char *)0x80193C12;
    }
    return var_a0_11;
}
