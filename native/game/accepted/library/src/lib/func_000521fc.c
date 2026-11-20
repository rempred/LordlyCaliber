typedef unsigned char u8;
typedef unsigned short u16;
typedef struct Node {
    struct Node *prev;
    struct Node *next;
    u8 unk08[0xE];
    u16 id;
} Node;
extern Node *D_8018FDC0;
void func_000521fc(Node *n);
void func_000016C4(void *p);
void func_000521fc(Node *n) {
    Node *next;
    Node *prev;
    if ((*(void * *)((u8 *)(n) + (0xD0))) != ((void *)0)) {
        func_000016C4((*(void * *)((u8 *)(n) + (0xD0))));
    }
    next = n->next;
    prev = n->prev;
    if (next != ((void *)0)) {
        next->prev = prev;
    }
    if (D_8018FDC0->next == n) {
        D_8018FDC0->next = next;
    } else {
        prev->next = n->next;
    }
    (*(u16 *)((u8 *)(D_8018FDC0) + (0)))--;
    if (n != ((void *)0)) {
        func_000016C4(n);
    }
}
