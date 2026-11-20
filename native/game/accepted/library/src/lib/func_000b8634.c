typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_80196AF8;
void bzero(void *p, s32 n);
u8 func_000b8634(void) {
    u8 used[30];
    s32 i;
    bzero(used, 30);
    for (i = 0; i < 30; i++) {
        u8 *q = D_80196AF8 + 0x117C + i * 0x36;
        if (q[3] != 0xFF) {
            used[q[0] - 1] = 1;
        }
    }
    for (i = 0; i < 30; i++) {
        if (used[i] == 0) {
            break;
        }
    }
    return i + 1;
}
