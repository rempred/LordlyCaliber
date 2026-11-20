typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct TextNode {
    struct TextNode *next;
    struct TextNode *prev;
    u8 data[0x70];
    u16 id;
    u8 value;
} TextNode;
extern TextNode *D_80190F4C;
u8 func_00050ce8(s32 id) {
    TextNode *p;
    for (p = D_80190F4C; p != ((void *)0); p = p->next) {
        if (p->id == id) {
            return p->value;
        }
    }
    return 0;
}
