typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
extern u8 *D_801AD100;
void func_000f8004(s16 id);
typedef struct {
    u16 flags;
    u8 flags2;
    u8 flags3;
    u8 unk04[0x10];
    void (*update)(void);
    void (*draw)(void);
    void (*exit)(void);
    u16 size;
    u8 unk22[2];
    s32 w24;
} CurScene;
extern CurScene D_800E7A30;
extern u8 D_800E91D0[];
void bzero(void *p, s32 n);
void func_000f809c(void);
void func_000f82bc(void);
void func_000f8004(s16 id) {
    D_800E7A30.update = func_000f809c;
    D_800E7A30.draw = func_000f82bc;
    D_800E7A30.size = 0x28;
    D_800E7A30.w24 = 0;
    D_800E7A30.flags |= 0x200;
    D_800E7A30.flags2 |= 9;
    D_800E7A30.flags3 |= 0xC;
    bzero(D_800E91D0, 0x28);
    *(s16 *)D_801AD100 = id;
}
void func_000f82bc(void);
