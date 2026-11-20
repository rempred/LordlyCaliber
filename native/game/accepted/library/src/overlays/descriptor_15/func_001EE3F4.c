typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
void func_00019e10(void);
typedef struct {
    u8 unk00[0x7E];
    u8 step;
} Seq1ED;
extern Seq1ED *D_801BA6D0;
typedef struct {
    u8 unk00[0x84];
    u8 *data;
    u8 unk88[0x100];
    s32 size;
} BgJob;
void func_001EDD54(s8 type, u16 w, u16 h, u8 *data, u8 a, u8 b);
void *func_00001330(s32 size);
void bzero(void *p, s32 n);
s32 func_001EE3F4(u8 *data, u8 a, u8 b) {
    u8 *p = data;
    s8 type = p[2];
    u16 w = *(u16 *)(p + 4);
    u16 h = *(u16 *)(p + 6);
    u8 *q;
    u8 *r;
    func_00019e10();
    p += 8;
    if (*(u16 *)data == 0x4855) {
        func_001EDD54(type, w, h, p, a, b);
        return 0;
    } else {
        D_801BA6D0 = func_00001330(0x190);
        bzero(D_801BA6D0, 0x190);
        ((u8 *)D_801BA6D0)[0x7E] = 0;
        ((BgJob *)D_801BA6D0)->data = data;
        ((u8 *)D_801BA6D0)[0x81] = 1;
        ((BgJob *)D_801BA6D0)->size = 0;
        ((u8 *)D_801BA6D0)[0x82] = a;
        ((BgJob *)D_801BA6D0)->size = 8;
        r = p;
        p += 0x10;
        ((BgJob *)D_801BA6D0)->size = *(u16 *)(r + 4) * *(u16 *)(r + 6) * 2 + 0x18;
        q = p;
        p += 8;
        func_001EDD54(q[2], *(u16 *)(q + 4), *(u16 *)(q + 6), p, 0, 1);
        return 1;
    }
}
