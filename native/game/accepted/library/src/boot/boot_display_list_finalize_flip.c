typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
typedef struct {
    void *p[5];
    Gfx *gfx;
} FbSet;
void boot_display_list_finalize_flip(void);
extern u8 D_800A81F0;
extern FbSet *D_800E9BE0;
extern u8 D_800C4808;
void func_00019c04(Gfx *start, s32 size, u8 a, s32 b);
void boot_display_list_counter_step(void);
extern Gfx *gTitleCardDisplayListCursor;
extern u8 D_80186E70[];
extern u8 D_801869C8[];
extern u8 g_resource_state_byte_3;
void boot_display_list_finalize_flip(void) {
    Gfx *g;
    boot_display_list_counter_step();
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xDE000000;
    g->w1 = (u32)D_801869C8;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xDE000000;
    g->w1 = (u32)D_80186E70;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xE7000000;
    g->w1 = 0;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xE9000000;
    g->w1 = 0;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xDF000000;
    g->w1 = 0;
    func_00019c04(D_800E9BE0->gfx, (gTitleCardDisplayListCursor - D_800E9BE0->gfx) * sizeof(Gfx), D_800C4808, 1);
    g_resource_state_byte_3 = 0;
    D_800A81F0 ^= 1;
}
void boot_display_list_counter_step(void);
