typedef unsigned char u8;
typedef signed int s32;
void bzero(void *p, s32 n);
extern u8 D_80197B00[];
extern u8 D_801936A8;
void func_00048268(void) {
    bzero(D_80197B00, 100);
    D_801936A8 = 0;
}
