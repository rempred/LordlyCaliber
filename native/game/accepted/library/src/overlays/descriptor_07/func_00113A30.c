typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_801AB850;
void func_00113A30(void) {
    u8 *s = D_801AB850;
    if (s[0x18] & 1) {
        if (*(s32 *)(s + 0x1C) < 4) {
            if (++*(s32 *)(s + 0x1C) == 4) {
                s[0x20] = 0;
            }
        }
    } else if (*(s32 *)(s + 0x1C) > 0) {
        (*(s32 *)(s + 0x1C))--;
    }
}
