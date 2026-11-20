typedef unsigned char u8;
typedef signed short s16;
typedef struct {
    u8 unk00[0xD0];
    u8 bD0;
} Ent23B;
Ent23B *D_801C8FE8(s16 a);
void func_0023B24C(s16 a) {
    D_801C8FE8(a)->bD0 = 1;
}
