typedef unsigned char u8;
typedef unsigned short u16;
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
typedef struct {
    void *unk0;
    void (*update)(void);
    void (*draw)(FbSet *);
} SceneFuncs;
extern SceneFuncs *D_800E8294;
extern u8 D_800C4800;
extern u8 D_800AEF98;
extern volatile u16 g_state_slot_status;
extern u8 D_800E810C;
extern u8 D_800E7A0C;
extern u8 D_800C4CE4;
extern u8 D_800A81FE;
extern u8 D_800A81FF;
extern u8 D_800A8200;
extern u8 D_800A8214;
extern u8 D_800A81FC;
extern u8 D_800AEFA0;
extern u32 D_800AEFA4;
extern u8 D_80196A28[];
u8 boot_status_flag_test(void);
void func_00019fc0(void);
void boot_resource_window_cache_update(void);
void D_800EAC24(void);
void D_800E9E34(void);
void D_800EAF1C(void);
void boot_resource_display_list_update(void);
void boot_display_list_sync_modes(void);
void boot_display_list_finalize_flip(void);
void boot_state_slot_render_callback_walk(void);
void D_80180BDC(void);
void boot_table_mask_reconcile(void);
void D_8016CB44(void *t);
void D_8007FD10(void);
void func_000071C8(void);
void boot_state_slot_callback_dispatch(void);
void D_8017F490(void);
void D_801841F4(void);
extern FbSet *D_800E9BE0;
extern Gfx *gTitleCardDisplayListCursor;
extern u8 D_801869C8[];
extern u8 D_800AEF99;
extern u32 D_800C4BCC;
extern s32 osTvType;
static void func_00002B10(void);
void boot_state_service_loop(s32 retrace) {
    if (D_800C4800 == 1) {
        D_800AEF98 = 1;
    } else if (D_800AEF98 == 0) {
        if (boot_status_flag_test()) {
            D_800AEF98 = 2;
        } else if (D_800AEF98 == 0 && g_state_slot_status == 0xFFFD) {
            D_800AEF98 = 3;
        }
    }
    if (D_800AEF99 != 0) {
        if (++D_800AEF99 > 12) {
            D_800AEF99 = 12;
            func_00019fc0();
            if (D_800AEF98 == 2 || D_800AEF98 == 3 || D_800AEF98 == 4) {
                if (retrace == 0) {
                    g_state_slot_status = 0xFFFC;
                }
            }
            return;
        }
    } else if (D_800AEF98 != 0) {
        D_800AEF99 = 1;
        boot_resource_window_cache_update();
    }
    if (g_state_slot_status == 0xFFFF || g_state_slot_status == 0xFFFD) {
        D_800EAC24();
        D_800E9E34();
        D_800EAF1C();
        if (D_800E810C && retrace < (D_800E7A0C & 1) + 1) {
            Gfx *g;
            boot_resource_display_list_update();
            boot_display_list_sync_modes();
            gTitleCardDisplayListCursor->w0 = 0xDE000000;
            gTitleCardDisplayListCursor->w1 = (u32)D_801869C8;
            gTitleCardDisplayListCursor++;
            if (D_800E8294->draw != ((void *)0)) {
                D_800E8294->draw(D_800E9BE0);
            }
            boot_state_slot_render_callback_walk();
            g = gTitleCardDisplayListCursor;
            gTitleCardDisplayListCursor = g + 1;
            g->w0 = 0xDE000000;
            g->w1 = (u32)D_801869C8;
            D_80180BDC();
            boot_display_list_finalize_flip();
            D_800C4CE4 = 1;
            D_800E810C = 0;
            D_800A81FF++;
        }
        if ((s32)(D_800C4BCC - D_800AEFA4) >= 2 || D_800A8214) {
            if (!D_800A81FC || D_800C4CE4) {
                boot_table_mask_reconcile();
                if (D_800AEFA0 == 0 || D_800AEFA0 == 0x83) {
                    D_8016CB44(&D_80196A28);
                    D_8007FD10();
                    if (D_800E8294->update != ((void *)0)) {
                        D_800E8294->update();
                    }
                    func_000071C8();
                    boot_state_slot_callback_dispatch();
                    D_8017F490();
                    D_801841F4();
                    D_800C4CE4 = 0;
                    D_800E810C = 1;
                    D_800A81FE += D_800C4BCC - D_800AEFA4;
                    D_800AEFA4 = D_800C4BCC;
                }
                if (D_800A81FE >= 60) {
                    D_800A8200 = D_800A81FF;
                    D_800A81FE = 0;
                    D_800A81FF = 0;
                }
            }
        }
    }
}
static void func_00002B10(void) {
    if (osTvType != 1 && osTvType != 2) {
        for (;;) {
        }
    }
}
