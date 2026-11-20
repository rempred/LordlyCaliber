typedef unsigned char u8;
typedef signed int s32;
extern u8 D_801FCFC0[][2];
void func_00146E54(u8 *o) {
    s32 lo;
    s32 hi;
    s32 mid;
    if (++o[5] >= 12) {
        o[5] = 0;
    }
    lo = 0;
    hi = 2;
    while (lo < hi) {
        mid = (lo + hi) / 2;
        if (o[5] < D_801FCFC0[mid + 1][0]) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    (*(s32 *)((u8 *)(o) + (0x10))) = (*(s32 * *)((u8 *)(o) + (0)))[D_801FCFC0[lo][1]];
    (*(s32 *)((u8 *)(o) + (0x14))) = (*(s32 * *)((u8 *)(o) + (0)))[D_801FCFC0[lo][1] + 3];
}
