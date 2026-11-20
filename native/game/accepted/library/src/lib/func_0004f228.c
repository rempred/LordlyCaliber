typedef unsigned char u8;
typedef unsigned int u32;
extern u32 *gTitleCardDisplayListCursor;
extern u8 D_8018FD10[];
void func_0004f228(void) {
    u32 *g = gTitleCardDisplayListCursor;
    gTitleCardDisplayListCursor = g + 2;
    g[0] = 0xDE000000;
    g[1] = (u32)D_8018FD10;
}
