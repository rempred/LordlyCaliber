typedef signed int s32;
extern s32 D_8022A8A8[];
void D_800EA86C(s32 ch, s32 v);
void D_800EA89C(s32 ch, s32 v);
void D_800EA8B4(s32 ch, s32 v);
void func_00283564(s32 i, s32 a, s32 b, s32 c) {
    s32 k = 0;
    s32 end;
    if (i == 7) {
        end = 7;
    } else {
        k = i;
        end = i + 1;
    }
    for (; k < end; k++) {
        if (a != -1) {
            D_800EA86C(D_8022A8A8[k], a);
        }
        if (b != -1) {
            D_800EA89C(D_8022A8A8[k], b);
        }
        if (c != -1) {
            D_800EA8B4(D_8022A8A8[k], c / 128);
        }
    }
}
