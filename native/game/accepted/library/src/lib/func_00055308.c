typedef unsigned char u8;
typedef unsigned short u16;
typedef struct Node {
    struct Node *prev;
    struct Node *next;
    u8 unk08[0xE];
    u16 id;
} Node;
extern Node *D_8018FDC0;
Node *func_000552c0(void);
extern inline Node *func_000552c0(void) {
    Node *n;
    if (D_8018FDC0 == ((void *)0)) {
        return ((void *)0);
    }
    n = D_8018FDC0->next;
    if (n != ((void *)0) && n->next != ((void *)0)) {
        do {
            n = n->next;
        } while (n->next != ((void *)0));
    }
    return n;
}
inline Node *func_00055308(void) {
    Node *n;
    if (D_8018FDC0 == ((void *)0)) {
        return ((void *)0);
    }
    if (D_8018FDC0->next == ((void *)0)) {
        return ((void *)0);
    }
    n = func_000552c0();
    while (n != ((void *)0)) {
        if (!((*(u8 *)((u8 *)(n) + (0x14))) & 0x10)) {
            return n;
        }
        n = (n->prev != D_8018FDC0->next) ? n->prev : ((void *)0);
    }
    return n;
}
