typedef unsigned char u8;
typedef unsigned short u16;
typedef struct Node {
    struct Node *prev;
    struct Node *next;
    u8 unk08[0xE];
    u16 id;
} Node;
extern Node *D_8018FDC0;
Node *func_0005526c(u16 id) {
    Node *n;
    Node *r = ((void *)0);
    for (n = D_8018FDC0->next; n != ((void *)0); n = n->next) {
        if (!(n->id & 0xFF00) && (n->id & 0xFF) == id) {
            r = n;
        } else if ((n->id & 0xFF00) == id) {
            r = n;
        }
    }
    return r;
}
