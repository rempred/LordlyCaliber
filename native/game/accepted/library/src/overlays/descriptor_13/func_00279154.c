typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern s32 e09_byte_80220F68;
extern s32 e09_pointer_80220F6C;
extern u8 D_800EB1F0[];
extern u8 D_80225828[];
extern u8 D_8022582E[];
void D_8020DFB8(void);
void D_8020DC80(u16 state);
void D_8020DD34(u16 state);
void D_8020E0B4(s32 a, s32 b);
void D_801F8E50(u8 id, u8 *s);
void D_800EA604(u8 *a, s32 b);
s32 func_00279154(u16 msg, u8 *o) {
    switch (msg) {
        case 0x101:
            D_8020DFB8();
            break;
        case 0x100:
            (*(s32 *)((u8 *)(o) + (0))) = e09_byte_80220F68;
            (*(s32 *)((u8 *)(o) + (4))) = e09_pointer_80220F6C;
            D_8020DC80(0x1002);
            break;
        case 0x1002:
            D_800EA604(D_800EB1F0, 0x2DE);
            D_801F8E50(*(*(u8 * *)((u8 *)(o) + (4))), D_80225828);
            D_8020E0B4(1, 0x1003);
            break;
        case 0x1003:
            D_801F8E50(*(*(u8 * *)((u8 *)(o) + (4))), D_8022582E);
            D_8020E0B4(0x50, 0x1004);
            break;
        case 0x1004:
            D_800EA604(D_800EB1F0, 0x2DF);
            D_8020E0B4(0x56, 0x1005);
            break;
        case 0x1005:
            D_8020DD34(0x101);
            break;
    }
    return 0;
}
