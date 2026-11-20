typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
void func_001BD430(u8 *p, u16 mask) {
    u16 bit = 1;
    u32 i;
    u8 *q;
    while (mask != 0) {
        if (mask & bit) {
            for (i = 0; i < 16; i++) {
                if ((*(u16 *)((u8 *)(p) + (0x12 + i * 6))) & bit) {
                    break;
                }
            }
            if (i < 16) {
                mask ^= bit;
                q = p + i * 6;
                (*(u16 *)((u8 *)(q) + (0x12))) = 0;
                q[0x14] = 0;
                q[0x15] = 0;
                q[0x16] = 0;
            }
        }
        bit <<= 1;
    }
}
