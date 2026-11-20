typedef signed int s32;
typedef float f32;
typedef struct {
    s32 m[16];
} Mtx29;
void guMtxL2F(f32 mf[4][4], Mtx29 *m);
void D_80233AE0(f32 x, f32 y, f32 z, f32 *out, f32 mf[4][4]);
void func_0029CAD8(Mtx29 m, s32 *x, s32 *y) {
    f32 mf[4][4];
    f32 out[3];
    guMtxL2F(mf, &m);
    *x = 0;
    *y = 0;
    D_80233AE0(0.0f, 0.0f, 0.0f, out, mf);
    if (out[0] < 2147483008.0f && -2147483008.0f < out[0]) {
        *x = out[0];
    }
    if (out[2] < 2147483008.0f && -2147483008.0f < out[2]) {
        *y = out[2];
    }
}
