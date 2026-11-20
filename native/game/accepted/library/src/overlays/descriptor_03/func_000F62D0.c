typedef unsigned char u8;
typedef signed int s32;
typedef float f32;
typedef struct {
    u8 unk000[0x104];
    s32 w104;
    u8 unk108[4];
    f32 f10C;
    u8 unk110[0x3E];
    u8 r;
    u8 g;
    u8 b;
    u8 unk151[0xB];
    f32 f15C;
} Scene0E;
extern Scene0E *D_801ABA58;
extern f32 D_800E7AD0[];
extern f32 D_800C4AD8[];
void func_00028900(f32 *mf, f32 a, f32 x, f32 y, f32 z);
void guMtxCatF(f32 *m, f32 *n, f32 *r);
void func_000F62D0(void) {
    func_00028900(&D_800E7AD0[2], D_801ABA58->f10C, 0.0f, 0.0f, 1.0f);
    guMtxCatF(&D_800E7AD0[2], &D_800C4AD8[2], &D_800C4AD8[2]);
}
