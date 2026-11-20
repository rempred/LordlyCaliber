typedef signed int s32;
void func_001BE168(s32 a, s32 b);
void func_001BF2E0(s32 a, s32 b);
void func_001BF2E0();
typedef unsigned char u8;
typedef unsigned int u32;
extern u8 D_8022A820;
typedef struct {
    u32 w0;
    u32 w1;
} GfxBB;
extern GfxBB *gTitleCardDisplayListCursor;
extern u8 D_8022A833;
extern u8 D_8022A0D0[];
static void func_001C2CE4(void);
void func_001C2CA8(s32 a, s32 b) {
    func_001BE168(a, b);
    func_001BF2E0(a, b);
}
static void func_001C2CE4(void) {
    GfxBB *g;
    if (D_8022A820) {
        g = gTitleCardDisplayListCursor++;
        g->w0 = 0xDE000000;
        g->w1 = (u32)D_8022A0D0;
        g = gTitleCardDisplayListCursor++;
        g->w0 = 0xFA000000;
        g->w1 = D_8022A833;
        g = gTitleCardDisplayListCursor++;
        g->w0 = 0xE7000000;
        g->w1 = 0;
        g = gTitleCardDisplayListCursor++;
        g->w0 = 0xE45003C0;
        g->w1 = 0;
        g = gTitleCardDisplayListCursor++;
        g->w0 = 0xE1000000;
        g->w1 = 0;
        g = gTitleCardDisplayListCursor++;
        g->w0 = 0xF1000000;
        g->w1 = 0x04000400;
        g = gTitleCardDisplayListCursor++;
        g->w0 = 0xE7000000;
        g->w1 = 0;
    }
}
