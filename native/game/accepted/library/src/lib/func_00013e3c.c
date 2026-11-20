typedef signed int s32;
typedef unsigned int u32;
void func_00013e3c(void *table, void *base, s32 n);
inline void func_00013e3c(void *table, void *base, s32 n) {
    u32 *p = table;
    u32 b = (u32)base;
    s32 i;
    for (i = 0; i < n; i++, p++) {
        if (*p != 0) {
            *p += b;
        }
    }
}
