typedef unsigned char u8;
typedef unsigned short u16;
typedef struct Node {
    struct Node *prev;
    struct Node *next;
    u8 unk08[0xE];
    u16 id;
} Node;
extern Node *D_8018FDC0_ovl_040E80;
Node *func_000552c0(void);
void func_000521fc(Node *n);
void func_00051dd4(void) {
    Node *n = func_000552c0();
    if (n != ((void *)0)) {
        do {
            func_000521fc(n);
            n = func_000552c0();
        } while (n != ((void *)0));
    }
}
void func_000521fc(Node *n);
extern inline Node *func_000552c0(void) {
    Node *n;
    if (D_8018FDC0_ovl_040E80 == ((void *)0)) {
        return ((void *)0);
    }
    n = D_8018FDC0_ovl_040E80->next;
    if (n != ((void *)0) && n->next != ((void *)0)) {
        do {
            n = n->next;
        } while (n->next != ((void *)0));
    }
    return n;
}
