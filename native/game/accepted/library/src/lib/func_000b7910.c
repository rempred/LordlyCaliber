typedef unsigned char u8;
typedef unsigned short u16;
extern u8 *D_80196AF8;
typedef struct {
    u8 b[0x36];
} OrgSquad;
u8 func_000bd154(u16 sq);
void func_000b7910(u16 sq, u16 v) {
    OrgSquad *q = &((OrgSquad *)(D_80196AF8 + 0x117C))[sq];
    if (q->b[4 + v] != 0xFF) {
        q->b[3] = v;
    }
    q->b[0x1D] = func_000bd154(sq);
}
