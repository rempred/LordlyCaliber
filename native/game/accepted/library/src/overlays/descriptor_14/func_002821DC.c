typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
extern Gfx *gTitleCardDisplayListCursor;
extern u8 D_8022A7D0[];
extern u8 D_8022A998;
void func_002821DC(void);
void func_002821DC(void);
s32 D_8023E050(void);
void D_8023C9EC(void);
void func_002821DC(void) {
    { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xDC080008); _g->w1 = ((u32)D_8022A7D0); };
    if (!D_8023E050()) {
        { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xE7000000); _g->w1 = (0); };
        { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xED000060); _g->w1 = (0x004FC35C); };
        { Gfx *_g = gTitleCardDisplayListCursor++; _g->w0 = (0xE7000000); _g->w1 = (0); };
    }
    if (D_8022A998 == 0) {
        D_8023C9EC();
    }
}
