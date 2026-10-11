int func_0029BA58(unsigned int arg0) {
    return ((arg0 >> 0x10) & 0xF800) | ((arg0 >> 0xD) & 0x7C0) | ((unsigned int) (arg0 & 0xF800) >> 0xA) | 1;
}
