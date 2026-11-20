typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
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
void func_0023EC00(void)
{
  Ctx23 *c = D_801EFE48;
  c->y0 = 0x14;
  c->y1 = 0xA2;
  c->x0 = 0x14;
  c->x1 = 0xF2;
  c->h652 = 0;
}
