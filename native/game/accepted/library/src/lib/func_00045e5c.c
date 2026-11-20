typedef unsigned char u8;
typedef signed int s32;
s32 func_00045e5c(u8 a, u8 b) {
    if (a == b) {
        return 1;
    }
    if ((u8)(b - 0x51) < 3 && (u8)(a - 0x51) < 3) {
        return 1;
    }
    if ((u8)(b - 0x54) < 2 && (u8)(a - 0x54) < 2) {
        return 1;
    }
    if ((u8)(b - 0x56) < 2 && (u8)(a - 0x56) < 2) {
        return 1;
    }
    if ((u8)(b - 0x5F) < 2 && (u8)(a - 0x5F) < 2) {
        return 1;
    }
    if ((u8)(b - 0x63) < 2 && (u8)(a - 0x63) < 2) {
        return 1;
    }
    return 0;
}
