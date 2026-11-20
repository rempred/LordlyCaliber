typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern u8 *D_8019A660;
void func_00019e10(void);
typedef struct {
    u8 unk00[0x84];
    u8 *data;
    u8 unk88[0x100];
    s32 size;
} BgJob_680F0;
void func_00068c88(s8 type, u16 w, u16 h, u8 *data, u8 a, u8 b);
void *func_00001330(s32 size);
void bzero(void *p, s32 n);
s32 func_00069328(u8 *data, u8 a, u8 b) {
    u8 *p = data;
    s8 type = p[2];
    u16 w = *(u16 *)(p + 4);
    u16 h = *(u16 *)(p + 6);
    u8 *q;
    u8 *r;
    func_00019e10();
    p += 8;
    if (*(u16 *)data == 0x4855) {
        func_00068c88(type, w, h, p, a, b);
        return 0;
    } else {
        D_8019A660 = func_00001330(0x190);
        bzero(D_8019A660, 0x190);
        ((u8 *)D_8019A660)[0x7E] = 0;
        ((BgJob_680F0 *)D_8019A660)->data = data;
        ((u8 *)D_8019A660)[0x81] = 1;
        ((BgJob_680F0 *)D_8019A660)->size = 0;
        ((u8 *)D_8019A660)[0x82] = a;
        ((BgJob_680F0 *)D_8019A660)->size = 8;
        r = p;
        p += 0x10;
        ((BgJob_680F0 *)D_8019A660)->size = *(u16 *)(r + 4) * *(u16 *)(r + 6) * 2 + 0x18;
        q = p;
        p += 8;
        func_00068c88(q[2], *(u16 *)(q + 4), *(u16 *)(q + 6), p, 0, 1);
        return 1;
    }
}
