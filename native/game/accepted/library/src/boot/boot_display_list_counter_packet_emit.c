typedef unsigned char u8;
typedef unsigned int u32;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
extern Gfx *gTitleCardDisplayListCursor;
extern u8 D_801869C8[];
void boot_display_list_counter_packet_emit(u8 alpha);
u8 D_8016CD30(void);
extern u8 D_80186358[];
extern u8 D_80186610[];
void boot_display_list_counter_packet_emit(u8 alpha) {
    if (alpha != 0) {
        {
            Gfx *g = gTitleCardDisplayListCursor;
            gTitleCardDisplayListCursor = g + 1;
            g[0].w0 = 0xE7000000;
            g[0].w1 = 0;
            gTitleCardDisplayListCursor = g + 2;
            g[1].w0 = 0xDE000000;
            g[1].w1 = (u32)D_801869C8;
            gTitleCardDisplayListCursor = g + 3;
            g[2].w0 = 0xD9000000;
            g[2].w1 = 0;
            gTitleCardDisplayListCursor = g + 4;
            g[3].w0 = 0xE7000000;
            g[3].w1 = 0;
            gTitleCardDisplayListCursor = g + 5;
            g[4].w0 = 0xE7000000;
            g[4].w1 = 0;
            gTitleCardDisplayListCursor = g + 6;
            g[5].w0 = 0xE3000A01;
            g[5].w1 = 0;
            gTitleCardDisplayListCursor = g + 7;
            g[6].w0 = 0xE7000000;
            g[6].w1 = 0;
            gTitleCardDisplayListCursor = g + 8;
            g[7].w0 = 0xFCFFFFFF;
            g[7].w1 = 0xFFFDF6FB;
            gTitleCardDisplayListCursor = g + 9;
            g[8].w0 = 0xE200001C;
            g[8].w1 = 0x504340;
            gTitleCardDisplayListCursor = g + 10;
            g[9].w0 = 0xFA000000;
            g[9].w1 = alpha;
        }
        if (!D_8016CD30()) {
            Gfx *g = gTitleCardDisplayListCursor;
            gTitleCardDisplayListCursor = g + 1;
            g[0].w0 = 0xE7000000;
            g[0].w1 = 0;
            gTitleCardDisplayListCursor = g + 2;
            g[1].w0 = 0xE45003C0;
            g[1].w1 = 0;
            gTitleCardDisplayListCursor = g + 3;
            g[2].w0 = 0xE1000000;
            g[2].w1 = 0;
            gTitleCardDisplayListCursor = g + 4;
            g[3].w0 = 0xF1000000;
            g[3].w1 = 0;
            gTitleCardDisplayListCursor = g + 5;
            g[4].w0 = 0xE7000000;
            g[4].w1 = 0;
        } else {
            Gfx *g = gTitleCardDisplayListCursor;
            gTitleCardDisplayListCursor = g + 1;
            g[0].w0 = 0xDE000000;
            g[0].w1 = (u32)D_80186358;
            gTitleCardDisplayListCursor = g + 2;
            g[1].w0 = 0xDE000000;
            g[1].w1 = (u32)D_80186610;
        }
        {
            Gfx *g = gTitleCardDisplayListCursor;
            gTitleCardDisplayListCursor = g + 1;
            g[0].w0 = 0xE7000000;
            g[0].w1 = 0;
        }
    }
}
