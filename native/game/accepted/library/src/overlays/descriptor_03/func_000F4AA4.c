typedef signed int s32;
typedef float f32;
f32 floor_f32(f32 x);
s32 func_000F4AA4(f32 a, f32 b) {
    a -= floor_f32(a);
    b -= floor_f32(b);
    if (b <= a) {
        return 0;
    }
    return 1;
}
