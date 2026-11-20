typedef signed short s16;
typedef signed int s32;
extern s16 D_80190F58;
extern s16 D_80190F5A;
extern s16 D_80190F5C;
extern s16 D_80190F5E;
void func_0004fe04(void);
void func_0004f2ac();
void func_0004f2ac(s32 unused, s32 x, s32 y) {
    D_80190F58 = x;
    D_80190F5C = x;
    D_80190F5A = y;
    D_80190F5E = y;
    func_0004fe04();
}
