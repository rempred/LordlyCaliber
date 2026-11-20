typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
void bzero(void *p, s32 size);
void *func_00001330(s32 size);
typedef struct {
    u8 next;
    u8 used;
    u8 unk02[0x12];
} Slot145;
extern s16 D_801FDA00;
extern u8 D_801FDA02;
extern Slot145 *D_801FDA54;
void func_00145B40(void) {
    s32 i;
    D_801FDA00 = 0x20;
    D_801FDA54 = func_00001330(0x280);
    bzero(D_801FDA54, 0x280);
    for (i = 0; i < D_801FDA00 - 1; i++) {
        D_801FDA54[i].used = 0;
        D_801FDA54[i].next = i + 1;
    }
    D_801FDA54[i].used = 0;
    D_801FDA54[i].next = 0xFF;
    D_801FDA02 = 0;
}
