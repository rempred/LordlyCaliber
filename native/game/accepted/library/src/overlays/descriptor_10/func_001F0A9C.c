typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    u8 unk0000[0x1C0];
    s32 strategy;
    u8 unk01C4[0x5020];
    s32 listB[0x36];
    s32 nB;
    s32 listA[0x100];
    s32 nA;
} Ctx1F;
typedef struct {
    void *data;
    s32 ttl;
    s32 id;
} Slot1F;
extern Ctx1F *D_801CE8BC;
void *func_001F0A9C(s32 id);
void *D_8009DD38(s32 id);
void *func_001F0A9C(s32 id) {
    Slot1F *s = (Slot1F *)D_801CE8BC;
    u32 i;
    for (i = 0; i < 16; i++) {
        if (s[i].id == id) {
            s[i].ttl = 15;
            return s[i].data;
        }
    }
    for (i = 0; i < 16; i++) {
        if ((s = (Slot1F *)D_801CE8BC + i)->id == 0) {
            s->id = id;
            s->ttl = 15;
            return (s = (Slot1F *)D_801CE8BC + i)->data = D_8009DD38(id);
        }
    }
    return ((void *)0);
}
