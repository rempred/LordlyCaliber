typedef unsigned char u8;
typedef signed int s32;
s32 func_0023683C(u8 a, u8 b) {
    s32 d;
    if (a == 0 || b == 0) {
        return 0;
    }
    if (a == 7 || b == 7) {
        return 0;
    }
    if (a == 8 || b == 8) {
        return 0;
    }
    if (a == 15 || b == 15) {
        return 0;
    }
    if ((u8)(a - 5) < 2) {
        if ((u8)(b - 5) < 2) {
            return (a != b) ? -1 : 1;
        }
        return 0;
    }
    if ((u8)(b - 5) < 2) {
        return 0;
    }
    if (a >= 9) {
        a = (a - 9) % 4 + 1;
    }
    if (b >= 9) {
        b = (b - 9) % 4 + 1;
    }
    d = a - b;
    if (d <= 0) {
        d = b - a;
    }
    return 1 - d;
}
