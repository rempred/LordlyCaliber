typedef struct Tree0E {
    struct Tree0E *left;
    struct Tree0E *right;
} Tree0E;
void *func_000016C4(void *p);
void func_000F07BC(Tree0E *n) {
    if (n != ((void *)0)) {
        func_000F07BC(n->left);
        func_000F07BC(n->right);
        func_000016C4(n);
    }
}
