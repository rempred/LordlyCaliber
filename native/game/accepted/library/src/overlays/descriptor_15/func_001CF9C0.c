typedef unsigned char u8;
typedef unsigned int u32;
extern u8 D_80243DB0[];
void func_00003798(void);
void func_00001314(void);
void resource_arena_init(void *p, u32 size);
void D_8009DAEC(void);
void D_80179060(void);
void D_8017BDC0(void);
void boot_resource_global_handle_slot_record_prepare(void);
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
extern Gfx *gTitleCardDisplayListCursor;
extern u8 D_801B80B0[];
static void func_001CFA18(void);
void func_001CF9C0(void) {
    func_00003798();
    func_00001314();
    resource_arena_init(D_80243DB0, 0x80400000 - (u32)D_80243DB0);
    D_8009DAEC();
    D_80179060();
    D_8017BDC0();
    boot_resource_global_handle_slot_record_prepare();
}
static void func_001CFA18(void) {
    Gfx *g;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xE7000000;
    g->w1 = 0;
    g = gTitleCardDisplayListCursor++;
    g->w0 = 0xDC080008;
    g->w1 = (u32)D_801B80B0;
}
