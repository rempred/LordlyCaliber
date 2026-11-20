typedef signed int s32;
s32 func_0001551c(void);
extern s32 *D_800A9B64[];
s32 func_0001551c(void) {
    s32 i;
    s32 max = 0;
    for (i = 0; D_800A9B64[i] != ((void *)0); i++) {
        if (max < D_800A9B64[i][1]) {
            max = D_800A9B64[i][1];
        }
    }
    return max;
}
