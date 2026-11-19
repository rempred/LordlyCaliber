typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile s32 vs32;
extern s32 D_80220F68;
extern s32 D_80220F6C;
extern u8 D_800EB1F0[];
extern u8 D_80225828_ovl_275850[];
extern u8 D_8022582E_ovl_275850[];
void func_8020DFB8(void);
void func_8020DC80(u16 state);
void func_8020DD34(u16 state);
void func_8020E0B4(s32 a, s32 b);
void func_801F8E50(u8 id, u8 *s);
void func_800EA604(u8 *a, s32 b);
void func_8020DF00(void *fn);
void func_80224CD0_ovl_275850();
extern u8 D_80225810_ovl_275850[];
extern u8 D_80225816_ovl_275850[];
extern u8 D_8022581C_ovl_275850[];
extern u8 D_80225822_ovl_275850[];
void func_80207F30(u8 a, s32 b, s32 c);
s32 func_80224830_ovl_275850(u16 msg, u8 *o) {
    switch (msg) {
        case 0x101:
            func_8020DFB8();
            break;
        case 0x100:
            (*(s32 *)((u8 *)(o) + (0))) = D_80220F68;
            (*(s32 *)((u8 *)(o) + (4))) = D_80220F6C;
            func_8020DC80(0x1002);
            break;
        case 0x1002:
            func_800EA604(D_800EB1F0, 0x2E0);
            func_801F8E50(*(*(u8 * *)((u8 *)(o) + (4))), D_80225810_ovl_275850);
            func_8020E0B4(9, 0x1003);
            break;
        case 0x1003:
            func_801F8E50(*(*(u8 * *)((u8 *)(o) + (4))), D_80225816_ovl_275850);
            func_8020E0B4(0x21, 0x1004);
            break;
        case 0x1004:
            func_801F8E50(*(*(u8 * *)((u8 *)(o) + (4))), D_8022581C_ovl_275850);
            func_8020E0B4(9, 0x1005);
            break;
        case 0x1005:
            func_801F8E50(*(*(u8 * *)((u8 *)(o) + (4))), D_80225822_ovl_275850);
            func_80207F30(*(*(u8 * *)((u8 *)(o) + (4))), 5, 0xF);
            func_8020E0B4(0x1E, 0x1006);
            break;
        case 0x1006:
            func_8020DD34(0x101);
            break;
    }
    return 0;
}
s32 func_802249C4_ovl_275850(u16 msg, u8 *o);
s32 func_80225080_ovl_275850(u16 msg, u8 *o);
