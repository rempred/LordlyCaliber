typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 b[0xA8];
} SceneSlot;
extern u8 D_8019EE50[];
extern SceneSlot D_800E82CA[];
s32 func_00007714(void *p);
void D_80079618();
void func_000496d0(void) {
    s32 i = func_00007714(D_8019EE50);
    if (i >= 0) {
        D_800E82CA[i].b[0] |= 2;
    }
    D_80079618(-0x11);
}
