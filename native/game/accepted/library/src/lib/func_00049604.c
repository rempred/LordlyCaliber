typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 b[0xA8];
} SceneSlot;
extern u8 D_8019EE50[];
extern SceneSlot D_800E82CA[];
void func_00003564(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g);
s32 func_0000735C(s32 a, void *b, s32 c, s32 d, s32 e, s32 f, s32 g);
void func_0004f1bc(void);
void func_0004ef60(void);
extern u8 D_8018F481;
void func_00049604(void) {
    func_0004f1bc();
    func_0004ef60();
    func_00003564(0, 2, 1, 0x200, 1, 0x80, 0x3700);
    D_800E82CA[func_0000735C(0xC000, D_8019EE50, 0, 0, 0x13F, 0xEF, 0xF000)].b[0x86] = D_8018F481;
}
