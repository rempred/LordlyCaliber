int func_00205720(unsigned int arg0) {
    return (arg0 >> 0x18) | ((arg0 >> 8) & 0xFF00) | ((arg0 & 0xFF00) << 8) | (arg0 << 0x18);
}
