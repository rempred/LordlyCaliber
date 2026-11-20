typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
extern u8 D_801FDA24;
extern Gfx *gTitleCardDisplayListCursor;
extern Gfx D_801FCF70[];
extern Gfx D_801FCF98[];
void func_00145E18(s32 on) {
    Gfx *g;
    if (on != 0) {
        if (D_801FDA24 != 1) {
            D_801FDA24 = 1;
            g = gTitleCardDisplayListCursor++;
            g->w0 = 0xDE000000;
            g->w1 = (u32)D_801FCF70;
        }
    } else if (D_801FDA24 != 2) {
        D_801FDA24 = 2;
        g = gTitleCardDisplayListCursor++;
        g->w0 = 0xDE000000;
        g->w1 = (u32)D_801FCF98;
    }
}
