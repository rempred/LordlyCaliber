typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
void D_801F95F8(s32 a, u8 *o);
void func_00147FA8(void);
void func_00147FA8(void);
void func_00149888(u8 *o) {
    switch (o[7]) {
        case 0:
            (*(u16 *)((u8 *)(o) + (4))) = ((*(u16 *)((u8 *)(o) + (4))) & 0xFFDF) | 0x6000;
            D_801F95F8(3, o);
            func_00147FA8();
            o[7] = 1;
            break;
        case 1:
            if ((*(u16 *)((u8 *)(o) + (4))) & 0x20) {
                o[7] = 2;
            }
            break;
        case 2:
            *(*(u32 * *)((u8 *)(o) + (0))) &= ~0x80000;
            func_00147FA8();
            o[0xB] = 0xFF;
            o[6] = 0;
            (*(u16 *)((u8 *)(o) + (4))) &= 0x9FFF;
            break;
    }
}
