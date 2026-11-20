typedef signed int s32;
s32 func_0028BB94(s32 a, s32 b) {
    switch (b & 0xF0) {
        case 0xA0:
            return (a & 0xF0) != 0xD0;
        case 0xB0:
            return (a & 0xF0) != 0xC0;
        case 0xC0:
            return (a & 0xF0) != 0xB0;
        case 0xD0:
            return (a & 0xF0) != 0xA0;
    }
    return 1;
}
