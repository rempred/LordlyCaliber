typedef signed int s32;
extern s32 D_801FDA10;
extern s32 *D_801FDA14;
extern s32 D_801FDA18;
void bzero(void *p, s32 size);
void *func_00001330(s32 size);
void func_00145CB0(void) {
    D_801FDA10 = 105;
    D_801FDA14 = func_00001330(0x1A4);
    bzero(D_801FDA14, 0x1A4);
    D_801FDA18 = 0;
}
