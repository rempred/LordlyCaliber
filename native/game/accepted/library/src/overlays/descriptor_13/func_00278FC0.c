typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern s32 e09_byte_80220F68;
extern s32 e09_pointer_80220F6C;
extern u8 D_800EB1F0[];
void D_8020DFB8(void);
void D_8020DC80(u16 state);
void D_8020DD34(u16 state);
void D_8020E0B4(s32 a, s32 b);
void D_801F8E50(u8 id, u8 *s);
void D_800EA604(u8 *a, s32 b);
extern u8 D_80225810[];
extern u8 D_80225816[];
extern u8 D_8022581C[];
extern u8 D_80225822[];
void D_80207F30(u8 a, s32 b, s32 c);
s32 func_00278FC0(u16 msg, u8 *o) {
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
            D_800EA604(D_800EB1F0, 0x2E0);
            D_801F8E50(*(*(u8 * *)((u8 *)(o) + (4))), D_80225810);
            D_8020E0B4(9, 0x1003);
            break;
        case 0x1003:
            D_801F8E50(*(*(u8 * *)((u8 *)(o) + (4))), D_80225816);
            D_8020E0B4(0x21, 0x1004);
            break;
        case 0x1004:
            D_801F8E50(*(*(u8 * *)((u8 *)(o) + (4))), D_8022581C);
            D_8020E0B4(9, 0x1005);
            break;
        case 0x1005:
            D_801F8E50(*(*(u8 * *)((u8 *)(o) + (4))), D_80225822);
            D_80207F30(*(*(u8 * *)((u8 *)(o) + (4))), 5, 0xF);
            D_8020E0B4(0x1E, 0x1006);
            break;
        case 0x1006:
            D_8020DD34(0x101);
            break;
    }
    return 0;
}
