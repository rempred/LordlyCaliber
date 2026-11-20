typedef unsigned char u8;
typedef unsigned short u16;
typedef struct Node28 {
    u8 unk00[4];
    struct Node28 *next;
    u8 unk08[0xC8];
    void *data;
} Node28;
typedef struct {
    u16 count;
    Node28 *first;
} List28;
extern List28 *D_8018FDC0;
void *func_000016C4(void *p);
void func_0028b1a4(void) {
    Node28 *n;
    Node28 *cur;
    if (D_8018FDC0 != ((void *)0)) {
        n = D_8018FDC0->first;
        while (n != ((void *)0)) {
            cur = n;
            if (cur->data != ((void *)0)) {
                func_000016C4(cur->data);
            }
            n = cur->next;
            func_000016C4(cur);
        }
        D_8018FDC0->count = 0;
        D_8018FDC0->first = ((void *)0);
    }
}
