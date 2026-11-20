typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    s32 w0;
    u8 unk04[4];
    s32 w8;
    u8 unk0C[0x18];
} Rec151;
extern u8 *D_80213670;
u8 func_00155D58(Rec151 *a, u8 id) {
    s8 j = -1;
    u32 i = 0;
    Rec151 *r;
    do {
        r = &a[(u8)i];
        if (r->w8 == id) {
            j++;
        }
        i++;
    } while (j != (s8)D_80213670[0x68]);
    return (*(u8 *)((u8 *)(r) + (3)));
}
