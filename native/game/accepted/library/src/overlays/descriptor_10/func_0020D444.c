typedef signed int s32;
s32 func_0020D444(s32 a, s32 b) {
    s32 q = b / 3;
    s32 n = 0;
    while (a > 0) {
        a -= q;
        n++;
    }
    a = 3;
    if (n < 4) {
        a = n;
    }
    return a;
}
