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
extern u8 D_8018F480;
extern Scene0E *D_801ABA58;
extern u8 D_801AC4D0[];
void D_80197FA4(s32 a, s32 b, u8 c);
void func_000F6258(void) {
    s32 k = (f32)(D_801ABA58->w104 & 0x1FF) / 512.0f * 340.0f;
    D_8018F480 = 0;
    D_80197FA4(0x2093576, 5, D_801AC4D0[k]);
}
