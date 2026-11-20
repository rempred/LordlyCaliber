typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_8022A248;
extern s32 D_8022A24C;
extern volatile s32 D_8022A250;
void bzero(void *p, s32 size);
void bzero(void *p, s32 n);
inline void func_001BA37C(void) {
    if (D_8022A248 != ((void *)0) && D_8022A24C != 0) {
        bzero(D_8022A248, D_8022A24C);
    }
    D_8022A250 = 0;
}
