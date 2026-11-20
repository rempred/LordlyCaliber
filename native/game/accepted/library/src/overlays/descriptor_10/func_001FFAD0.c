typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
extern s16 D_801D07F4;
void bzero(void *p, s32 n);
typedef struct {
    f32 pos[3];
    f32 dir[3];
    f32 scale[3];
    u8 rgba[4];
    s32 on;
    u8 unk2C[4];
} Light1F8;
extern Light1F8 D_801D0774[2];
void func_001FFAD0(void) {
    D_801D0774[0].rgba[0] = D_801D0774[0].rgba[1] = D_801D0774[0].rgba[2] = D_801D0774[0].rgba[3] = 0xFF;
    D_801D0774[0].on = 1;
    D_801D0774[1].rgba[0] = D_801D0774[1].rgba[1] = D_801D0774[1].rgba[2] = D_801D0774[1].rgba[3] = 0xFF;
    D_801D0774[1].on = 1;
    D_801D0774[0].pos[0] = D_801D0774[0].pos[1] = 0.0f;
    D_801D0774[0].dir[0] = D_801D0774[0].dir[1] = D_801D0774[0].dir[2] = 0.0f;
    D_801D0774[1].pos[0] = D_801D0774[1].pos[1] = 0.0f;
    D_801D0774[1].dir[0] = D_801D0774[1].dir[1] = D_801D0774[1].dir[2] = 0.0f;
    D_801D0774[0].pos[2] = -5.0f;
    D_801D0774[0].scale[0] = D_801D0774[0].scale[1] = D_801D0774[0].scale[2] = 1.0f;
    D_801D0774[1].pos[2] = -905.0f;
    D_801D0774[1].scale[0] = D_801D0774[1].scale[1] = D_801D0774[1].scale[2] = 1.0f;
    bzero(&D_801D07F4, 0x1A);
}
