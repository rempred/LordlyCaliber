typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef struct {
    u8 unk00[4];
    s16 h4;
    s16 h6;
    s16 h8;
    u8 unk0A[2];
    s16 hC;
    s16 hE;
    s16 h10;
    u8 marks[0x10];
    u8 zero[0x10];
    u8 unk32[0x14];
    u8 b46;
    u8 b47;
} Panel1F;
void *func_0002CD70(void *p, s32 c, s32 n);
void bzero(void *p, s32 n);
void func_001F0E04(Panel1F *p, s16 a) {
    p->h4 = a;
    p->h6 = -1;
    p->h8 = 0;
    p->h10 = 0;
    p->hE = 0;
    p->hC = 0;
    p->b46 = 0;
    p->b47 = 0;
    func_0002CD70(p->marks, 0xFF, 0x10);
    bzero(p->zero, 0x10);
}
