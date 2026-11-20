typedef unsigned char u8;
typedef unsigned int u32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
void boot_display_list_state_emit(void);
extern Gfx *gTitleCardDisplayListCursor;
extern u8 D_801869C8[];
extern u8 g_resource_state_byte_3;
extern u32 g_resource_state_word;
extern void *D_800E8210;
u32 osVirtualToPhysical(void *p);
u8 D_8016CD30(void);
void boot_display_list_state_emit(void) {
    if (D_8016CD30()) {
        return;
    }
    if (g_resource_state_byte_3) {
        Gfx *g = gTitleCardDisplayListCursor;
        gTitleCardDisplayListCursor = g + 1;
        g[0].w0 = 0xFE000000;
        g[0].w1 = g_resource_state_word - 0x80000000;
        gTitleCardDisplayListCursor = g + 2;
        g[1].w0 = 0xE7000000;
        g[1].w1 = 0;
        gTitleCardDisplayListCursor = g + 3;
        g[2].w0 = 0xE7000000;
        g[2].w1 = 0;
        gTitleCardDisplayListCursor = g + 4;
        g[3].w0 = 0xE3000A01;
        g[3].w1 = 0x300000;
        gTitleCardDisplayListCursor = g + 5;
        g[4].w0 = 0xE7000000;
        g[4].w1 = 0;
        gTitleCardDisplayListCursor = g + 6;
        g[5].w0 = 0xE200001C;
        g[5].w1 = 0;
        gTitleCardDisplayListCursor = g + 7;
        g[6].w0 = 0xFF10013F;
        g[6].w1 = g_resource_state_word - 0x80000000;
        gTitleCardDisplayListCursor = g + 8;
        g[7].w0 = 0xF7000000;
        g[7].w1 = 0xFFFCFFFC;
        gTitleCardDisplayListCursor = g + 9;
        g[8].w0 = 0xE7000000;
        g[8].w1 = 0;
        gTitleCardDisplayListCursor = g + 10;
        g[9].w0 = 0xF64FC3BC;
        g[9].w1 = 0;
        gTitleCardDisplayListCursor = g + 11;
        g[10].w0 = 0xE7000000;
        g[10].w1 = 0;
        gTitleCardDisplayListCursor = g + 12;
        g[11].w0 = 0xE7000000;
        g[11].w1 = 0;
    }
    {
        Gfx *g = gTitleCardDisplayListCursor;
        gTitleCardDisplayListCursor = g + 1;
        g[0].w0 = 0xE7000000;
        g[0].w1 = 0;
        gTitleCardDisplayListCursor = g + 2;
        g[1].w0 = 0xE7000000;
        g[1].w1 = 0;
        gTitleCardDisplayListCursor = g + 3;
        g[2].w0 = 0xE3000A01;
        g[2].w1 = 0x300000;
        gTitleCardDisplayListCursor = g + 4;
        g[3].w0 = 0xE7000000;
        g[3].w1 = 0;
        gTitleCardDisplayListCursor = g + 5;
        g[4].w0 = 0xE200001C;
        g[4].w1 = 0;
        gTitleCardDisplayListCursor = g + 6;
        g[5].w0 = 0xFF10013F;
        g[5].w1 = osVirtualToPhysical(D_800E8210);
    }
    {
        Gfx *g = gTitleCardDisplayListCursor;
        gTitleCardDisplayListCursor = g + 1;
        g[0].w0 = 0xF7000000;
        g[0].w1 = 0x10001;
        gTitleCardDisplayListCursor = g + 2;
        g[1].w0 = 0xE7000000;
        g[1].w1 = 0;
        gTitleCardDisplayListCursor = g + 3;
        g[2].w0 = 0xF64FC3BC;
        g[2].w1 = 0;
        gTitleCardDisplayListCursor = g + 4;
        g[3].w0 = 0xE7000000;
        g[3].w1 = 0;
        gTitleCardDisplayListCursor = g + 5;
        g[4].w0 = 0xDE000000;
        g[4].w1 = (u32)D_801869C8;
    }
}
