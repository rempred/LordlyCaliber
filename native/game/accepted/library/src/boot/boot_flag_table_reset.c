typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern u16 D_800AEF10[4][16];
extern u16 D_800AEE90[4][16];
typedef struct {
    u8 b[6];
} Unk6;
extern Unk6 D_800AEE78[4];
void bzero(void *p, s32 n);
extern u16 D_800C4BF0;
extern u8 D_800AEF9A;
static void func_00002C4C(void);
void boot_flag_table_reset(void) {
    s32 i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 16; j++) {
            D_800AEE90[i][j] = D_800AEF10[i][j] = 0;
        }
    }
    bzero(D_800AEE78, 0x18);
}
static void func_00002C4C(void) {
    u8 f = D_800AEF9A;
    if ((f & 1) && !(f & 4)) {
        if (f & 2) {
            if (D_800C4BF0 == 0) {
                D_800AEF9A = f & ~2;
            }
        } else if (D_800C4BF0 == 0xD020 || D_800C4BF0 == 0xF000) {
            D_800AEF9A = f | 6;
        }
    }
}
