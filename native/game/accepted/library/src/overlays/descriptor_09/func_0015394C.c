typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
extern Gfx *gTitleCardDisplayListCursor;
extern u8 *D_801F0B90;
extern u8 D_80210258[];
extern u8 D_80213648[];
void func_001520E0(void *a, void *b, s32 c, s32 d, s32 e);
void func_0015394C(void) {
    Gfx *g = gTitleCardDisplayListCursor++;
    g->w0 = 0xDE000000;
    g->w1 = (u32)D_80213648;
    func_001520E0(D_80210258, D_801F0B90 + 0x948, 0x40, 0x40, 2);
}
