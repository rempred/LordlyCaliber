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
typedef struct {
    s32 w[0x58 / 4];
} Blk58;
extern Blk58 D_8022A720;
void func_00282508(void);
void func_002825BC(void);
void D_802267DC(void);
void func_0028240C(u16 id) {
    D_800E7A44.update = func_00282508;
    D_800E7A44.draw = func_002825BC;
    D_800E7A44.exit = D_802267DC;
    D_800E7A44.size = 0x5C;
    D_800E7A54 = 0;
    D_800E7A32 |= 9;
    D_800E7A33 |= 5;
    bzero(D_800E91D0, 0x5C);
    *(s32 *)D_800E91D0 = id;
    *(Blk58 *)(D_800E91D0 + 4) = D_8022A720;
}
void func_00282508(void);
void D_802267DC(void);
