typedef signed int s32;
typedef unsigned int u32;
extern s32 D_800A81F4;
extern u32 D_800A81F8;
extern u32 D_800C4BCC;
extern s32 D_800EB0DC;
extern s32 D_800EB12C;
extern s32 D_800EB17C;
extern s32 D_800EB1CC;
extern s32 D_800EB21C;
extern s32 D_800EB26C;
extern s32 D_800EB2BC;
void func_00011d08(s32 a, s32 b);
static s32 func_00004358(void);
void boot_resource_window_cache_update(void) {
    if (D_800A81F4 == 0) {
        D_800EB0DC = 0;
        D_800EB12C = 0;
        D_800EB17C = 0;
        D_800EB1CC = 0;
        D_800EB21C = 0;
        D_800EB26C = 0;
        D_800EB2BC = 0;
        func_00011d08(3, 12);
        D_800A81F4 = 12;
        D_800A81F8 = D_800C4BCC;
    }
}
static s32 func_00004358(void) {
    u32 start = D_800A81F8;
    u32 end = start + 60;
    if (D_800A81F4 == 0) {
        return 0;
    }
    if (start < end) {
        if (D_800C4BCC >= end || D_800C4BCC < start) {
            D_800A81F4 = 0;
        }
    } else if (D_800C4BCC > end && D_800C4BCC < start) {
        D_800A81F4 = 0;
    }
    return D_800A81F4;
}
