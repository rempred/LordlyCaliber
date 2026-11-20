typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern u8 D_80239140[];
extern u8 D_8023914C[];
void D_8020DFB8(void);
void D_8020DC80(u16 state);
void D_8020E0B4(s32 a, s32 b);
void D_8020B538(s32 a, u8 *s);
s32 func_002A7A6C(u16 msg) {
    switch (msg) {
        case 0x101:
            D_8020DFB8();
            break;
        case 0x100:
            D_8020DC80(0x1010);
            break;
        case 0x1010:
            D_8020E0B4(10, 0x1100);
            break;
        case 0x1100:
            D_8020B538(4, D_80239140);
            D_8020E0B4(10, 0x1200);
            break;
        case 0x1200:
            D_8020B538(4, D_8023914C);
            D_8020E0B4(10, 0x101);
            break;
    }
    return 0;
}
