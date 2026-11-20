typedef unsigned char u8;
typedef signed int s32;
typedef float f32;
extern s32 D_8022A990;
extern u8 D_8018FC19;
extern s32 D_8022A720;
extern s32 D_8022A734;
extern s32 D_8022A738;
extern s32 D_8022A73C;
extern s32 D_8022A740;
extern s32 D_8022A744;
extern s32 D_8022A748;
extern s32 D_8022A778;
extern s32 D_8022A78C;
extern s32 D_8022A790;
extern s32 D_8022A794;
extern s32 D_8022A798;
extern s32 D_8022A79C;
extern s32 D_8022A7A0;
void func_00284288(void);
void func_00282FE0(void) {
    if (!D_8018FC19) {
        D_8022A740 = 0;
        D_8022A744 = 0;
        D_8022A748 = 0;
        D_8022A738 = 0;
        D_8022A73C = 0;
        D_8022A798 = 0;
        D_8022A79C = 0;
        D_8022A7A0 = 0;
        D_8022A78C = 0;
        D_8022A790 = 0;
        *(f32 *)&D_8022A734 = 360.0f;
        *(f32 *)&D_8022A720 = 38.0f;
        *(f32 *)&D_8022A794 = 360.0f;
        *(f32 *)&D_8022A778 = 38.0f;
    }
    D_8022A990 = 0;
    func_00284288();
}
