typedef signed short s16;
typedef signed int s32;
extern s32 D_801D07E4;
extern s32 D_801D07E8;
extern s32 D_801D07EC;
void func_001FF5EC(s16 a, s16 b, s32 c, s32 d, s32 e);
void func_001FF7EC(s16 a, s16 b) {
    func_001FF5EC(a, b, D_801D07E8, D_801D07EC, D_801D07E4);
}
