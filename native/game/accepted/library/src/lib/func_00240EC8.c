typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 unk00[0x60];
    u16 flags;
    u8 unk62[4];
    s16 id;
} Ent23;
typedef struct {
    Ent23 ent[4];
    u8 unk1A0[0x1D0];
    s16 x0;
    s16 x1;
    s16 y0;
    s16 y1;
    u8 unk378[0x2D8];
    u16 flags;
    s16 h652;
    u8 unk654[2];
    s16 fade;
} Ctx23;
extern Ctx23 *D_801EFE48;
void func_0023F804(void);
void func_00240048(void);
void func_0024027C(void);
void func_00240D20(void);
void func_00240EC8(void) {
    s32 i;
    u8 *o;
    if (D_801EFE48->flags & 2) {
        func_0023F804();
        for (i = 0; i < 7; i++) {
            o = (u8 *)D_801EFE48 + 0x378 + i * 0x54;
            (*(void (**)(u8 *))o)(o);
        }
        func_00240048();
        func_0024027C();
        func_00240D20();
    }
}
