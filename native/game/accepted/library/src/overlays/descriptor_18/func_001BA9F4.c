typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 used;
    u8 value;
    u16 id;
} ScriptSlot;
extern ScriptSlot D_8022A1F0[];
extern s32 *D_8022A214;
void *func_001C216C(s32 i, void *text, s32 x, s32 y, u8 flags);
s32 func_001BA9F4(u8 *ctx) {
    s32 k = *(s32 *)(*(u8 **)(ctx + 8) + 8);
    u8 *cmd;
    u8 *text;
    s32 i;
    u16 id;
    u8 v;
    if (D_8022A214 == ((void *)0)) {
        for (;;);
    }
    text = (u8 *)D_8022A214 + D_8022A214[k];
    func_001C216C((*(u8 **)(ctx + 8))[3], text,
                             *(s16 *)(*(u8 **)(ctx + 8) + 4), *(s16 *)(*(u8 **)(ctx + 8) + 6), (*(u8 **)(ctx + 8))[1]);
    cmd = *(u8 **)(ctx + 8);
    id = cmd[3] | 0x5400;
    v = cmd[2] + 1;
    for (i = 0; i < 8; i++) {
        if (D_8022A1F0[i].used == 0) {
            break;
        }
    }
    if (i >= 8) {
        for (;;) {
        }
    }
    D_8022A1F0[i].id = id;
    D_8022A1F0[i].value = v;
    D_8022A1F0[i].used = 1;
    *(u8 **)(ctx + 8) += 12;
    return 0;
}
