typedef unsigned char u8;
typedef signed int s32;
typedef float f32;
void func_000F309C(f32 lo, f32 hi, f32 x, f32 *from, f32 *to, u8 *out) {
    s32 i;
    s32 c;
    f32 v;
    if (x < lo) {
        x = lo;
    }
    if (hi < x) {
        x = hi;
    }
    for (i = 0; i < 3; i++) {
        v = to[i];
        if (v != from[i]) {
            v = (v - from[i]) / (hi - lo) * (x - lo) + from[i];
        }
        c = v * 256.0f;
        if (c >= 256) {
            c = 255;
        }
        out[i] = c;
    }
}
