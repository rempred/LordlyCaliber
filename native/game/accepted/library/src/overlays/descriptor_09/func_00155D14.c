typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    s32 w0;
    u8 unk04[4];
    s32 w8;
    u8 unk0C[0x18];
} Rec151;
u8 func_00155D14(Rec151 *r, u8 k) {
    u8 n = 0;
    u8 i = 0;
    Rec151 *e;
    do {
        e = &r[i];
        if (e->w8 == k) {
            n++;
        }
        i++;
    } while (e->w0 != -1);
    return n;
}
