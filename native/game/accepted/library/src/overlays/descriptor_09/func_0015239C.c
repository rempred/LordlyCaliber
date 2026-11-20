typedef unsigned int u32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
extern Gfx *gTitleCardDisplayListCursor;
void func_0015239C(u32 pal) {
    Gfx *g;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xFD100000;
    g->w1 = pal;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xE8000000;
    g->w1 = 0;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xF5000100;
    g->w1 = 0x07000000;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xE6000000;
    g->w1 = 0;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xF0000000;
    g->w1 = 0x073FC000;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xE7000000;
    g->w1 = 0;
}
