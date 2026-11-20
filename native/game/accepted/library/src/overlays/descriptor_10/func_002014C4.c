typedef unsigned char u8;
u8 func_002014C4(u8 a) {
    if (a < 36) {
        return a + 55;
    }
    if ((u8)(a - 36) < 3) {
        return a + 118;
    }
    if ((u8)(a - 39) < 54) {
        return a + 52;
    }
    if ((u8)(a - 97) < 28) {
        return a + 176;
    }
    if ((u8)(a - 125) < 8) {
        return a + 21;
    }
    return 1;
}
