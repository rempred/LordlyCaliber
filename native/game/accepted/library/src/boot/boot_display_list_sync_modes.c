typedef unsigned int u32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
void boot_display_list_sync_modes(void);
extern Gfx *gTitleCardDisplayListCursor;
void func_00025a10(u32 f);
void boot_display_list_sync_modes(void) {
    Gfx *g;
    func_00025a10(0x5A);
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xE7000000;
    g->w1 = 0;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xE3001801;
    g->w1 = 0;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xE3001A01;
    g->w1 = 0x30;
}
