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
extern TextNode *D_80190F48;
u8 *func_00050c3c(s32 id);
u8 *func_00050c3c(s32 id) {
    TextNode *n;
    for (n = D_80190F4C; n != ((void *)0); n = n->next) {
        if (n->id == id) {
            if (n->next == ((void *)0)) {
                TextNode *prev = n->prev;
                TextNode *old = D_80190F4C;
                D_80190F4C = n;
                D_80190F48 = prev;
                prev->next = ((void *)0);
                old->prev = n;
                n->prev = ((void *)0);
                n->next = old;
            } else if (n->prev != ((void *)0)) {
                TextNode *prev;
                TextNode *next;
                TextNode *old;
                n->next->prev = n->prev;
                prev = n->prev;
                next = n->next;
                old = D_80190F4C;
                D_80190F4C = n;
                prev->next = next;
                old->prev = n;
                n->next = old;
                n->prev = ((void *)0);
            }
            return n->data;
        }
    }
    return ((void *)0);
}
