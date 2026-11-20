typedef unsigned char u8;
typedef signed short s16;
extern s16 D_800E7A36;
extern s16 D_800E7A38;
extern s16 D_800E7A3A;
extern s16 D_800E7A3C;
extern u8 *D_801AD100;
extern void *gTitleCardDisplayListCursor;
void func_00008A74(s16 a, s16 b, s16 c, s16 d);
void emit_rdp_setup_displaylist(void *gfx, void *p);
void func_000f82bc(void);
void func_000f82bc(void) {
    func_00008A74(D_800E7A36, D_800E7A38, D_800E7A3A, D_800E7A3C);
    emit_rdp_setup_displaylist(&gTitleCardDisplayListCursor, D_801AD100 + 4);
}
