typedef unsigned char u8;
typedef signed int s32;
typedef float f32;
extern u8 D_800E7A90[];
f32 D_801A63FC(s32 slot, f32 x, f32 z);
extern f32 D_801F0D98;
extern f32 D_801F0D9C;
extern f32 D_801F0DA0;
extern f32 D_801F0DA4;
void func_0012EBB8(s32 gx, s32 gz, f32 *o) {
    f32 v[3];
    v[0] = D_801F0D98 + (D_801F0DA0 - D_801F0D98) * gx / 256.0f;
    v[2] = D_801F0D9C + (D_801F0DA4 - D_801F0D9C) * gz / 256.0f;
    *o++ = v[0];
    *o = D_801A63FC(D_800E7A90[0x29], v[0], v[2]);
    o[1] = v[2];
}
