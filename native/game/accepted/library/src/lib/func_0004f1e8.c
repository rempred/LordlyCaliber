typedef unsigned char u8;
typedef unsigned int u32;
extern u32 *gTitleCardDisplayListCursor;
extern u8 D_8018FC80[];
void func_0004f254(void);
void func_0004f254();
void func_0004f1e8(void) {
    u32 *g = gTitleCardDisplayListCursor;
    gTitleCardDisplayListCursor = g + 2;
    g[0] = 0xDE000000;
    g[1] = (u32)D_8018FC80;
    func_0004f254();
}
void func_0004f254(void);
