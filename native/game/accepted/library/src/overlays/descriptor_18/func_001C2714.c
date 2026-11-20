typedef signed short s16;
typedef signed int s32;
void bzero(void *p, s32 size);
extern s32 D_8022A720;
extern s16 D_8022A724;
extern s16 D_8022A726;
extern s16 D_8022A728;
extern s16 D_8022A72A;
void func_001C23EC(s16 *p);
inline void func_001C2714(s16 *list) {
    if (list == ((void *)0)) {
        s32 *p = &D_8022A720;
        bzero(p, 0xF4);
        *p = 1;
        D_8022A72A = 0xF0;
        D_8022A726 = 0;
        D_8022A724 = 0;
        D_8022A728 = 0x140;
    } else {
        s16 *q = list;
        while (q[1] != 0 || q[3] != 0 || q[0] != 0 || q[2] != 0) {
            func_001C23EC(q);
            q += 4;
        }
    }
}
