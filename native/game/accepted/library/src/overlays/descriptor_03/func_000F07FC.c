typedef unsigned char u8;
typedef signed int s32;
typedef float f32;
f32 *D_80080040(f32 *out, f32 *a, f32 *b);
f32 *D_8007FEE0(f32 *out, f32 *a, f32 *b);
void func_000f6810(f32 a, f32 b, f32 *p, f32 *q, f32 *axis, f32 c, s32 d);
void func_000F07FC(s32 arg, u8 *o) {
    f32 d[3];
    f32 c[3];
    f32 n[3];
    D_80080040(d, (f32 *)(o + 0x20), (f32 *)(o + 0x14));
    D_8007FEE0(c, (f32 *)(o + 0x2C), d);
    D_8007FEE0(n, d, c);
    func_000f6810((*(f32 *)((u8 *)(o) + (0))), (*(f32 *)((u8 *)(o) + (4))), (f32 *)(o + 0x14), (f32 *)(o + 0x20), n,
                             (*(f32 *)((u8 *)(o) + (0xC))), arg);
}
