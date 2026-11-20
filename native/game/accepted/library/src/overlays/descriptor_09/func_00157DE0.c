typedef signed char s8;
typedef unsigned char u8;
void D_80179018(void);
extern u8 *D_80213778;
extern u8 *D_80213778;
void func_00157DE0(void) {
    u8 v;
    v = D_80213778[0x5F3];
    if ((s8)(v & 0x7F) >= 0x10) {
        D_80213778[0x5F3] = v & 0x80;
        D_80213778[0x5F3] ^= 0x80;
    }
    D_80213778[0x5F3]++;
    if (D_80213778[0x5F3] & 0x80) {
        D_80213778[0x5F4] += 2;
    } else {
        D_80213778[0x5F4] -= 2;
    }
    v = D_80213778[0x5F5];
    if (v & 0x80) {
        if (v & 0xF) {
            D_80213778[0x5F5] = v - 1;
            D_80213778[0x5F5] |= 0x80;
        } else {
            D_80213778[0x5F5] = 0;
        }
    } else if ((v & 0xF) < 4) {
        D_80213778[0x5F5] = v + 1;
        D_80213778[0x5F5] &= 0x7F;
    } else {
        D_80213778[0x5F5] = 0x84;
    }
    D_80179018();
}
