typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
void D_801F95F8(s32 a, u8 *o);
void func_00149954(u8 *o) {
    switch (o[7]) {
        case 0:
            (*(u16 *)((u8 *)(o) + (4))) = ((*(u16 *)((u8 *)(o) + (4))) & 0xFFBF) | 0x6000;
            D_801F95F8(4, o);
            o[7] = 1;
            break;
        case 1:
            if (!((*(u16 *)((u8 *)(o) + (4))) & 0x40)) {
                break;
            }
            o[7] = 2;
        case 2:
            o[0x31] &= 0xFE;
            if (*(*(u32 * *)((u8 *)(o) + (0))) & 0x80000) {
                o[6] = 3;
                o[7] = 0;
            }
            break;
    }
}
