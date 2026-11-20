typedef unsigned char u8;
typedef unsigned short u16;
typedef struct Node {
    struct Node *prev;
    struct Node *next;
    u8 unk08[0xE];
    u16 id;
} Node;
extern Node *D_8018FDC0;
Node *func_00055234(u16 id) {
    Node *found = ((void *)0);
    Node *n;
    for (n = D_8018FDC0->next; n != ((void *)0); n = n->next) {
        if (n->id == id) {
            found = n;
        }
    }
    return found;
}
