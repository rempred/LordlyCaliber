typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    void (*update)(void);
    void (*draw)(void);
    void (*exit)(void);
    u16 size;
} SceneFuncs;
extern SceneFuncs D_800E7A44;
extern s32 D_800E7A54;
extern u8 D_800E7A32;
extern u8 D_800E7A33;
extern u8 D_800E91D0[];
void bzero(void *p, s32 n);
void D_80226190(void);
void func_002821DC(void);
void D_802263C4(s32 arg);
extern u8 D_8022A980;
void D_80226190(void);
void func_002821DC(void);
void overlay_29a4c0_bss_end(void);
void D_80226190(void);
void func_002821DC(void);
void func_002822F4(void) {
    D_800E7A44.update = D_80226190;
    D_800E7A44.draw = func_002821DC;
    D_800E7A44.exit = (void (*)(void))D_802263C4;
    D_800E7A44.size = 0x5C;
    D_800E7A54 = 0;
    D_800E7A32 |= 9;
    D_800E7A33 |= 1;
    bzero(D_800E91D0, 0x5C);
    D_8022A980 = 1;
    overlay_29a4c0_bss_end();
}
void D_802263C4(s32 arg);
