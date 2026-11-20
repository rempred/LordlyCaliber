typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 b[0x34];
} EChar282;
typedef struct {
    u8 b[0x38];
} PChar282;
extern EChar282 D_80195560_copy[];
extern PChar282 D_80193BC0_copy[];
extern void *D_8018EA00[];
void *func_00283808(s32 id) {
    s32 i;
    for (i = 0; i < 100; i++) {
        if (D_80195560_copy[i].b[0x11] == id) {
            return &D_80195560_copy[i];
        }
    }
    for (i = 0; i < 100; i++) {
        if (D_80193BC0_copy[i].b[0x11] == id) {
            return &D_80193BC0_copy[i];
        }
    }
    if (id >= 0x54) {
        return D_8018EA00[id];
    }
    return ((void *)0);
}
