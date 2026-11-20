typedef float f32;
f32 func_0029EAB0(f32 a);
f32 func_0029EAB0(f32 a) {
    for (;;) {
        if (a < 0.0f) {
            a += 360.0f;
        } else if (a >= 360.0f) {
            a -= 360.0f;
        } else {
            break;
        }
    }
    return a;
}
