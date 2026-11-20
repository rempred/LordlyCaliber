typedef unsigned char u8;
typedef unsigned short u16;
typedef float f32;
typedef struct Tree0E {
    struct Tree0E *left;
    struct Tree0E *right;
} Tree0E;
Tree0E *func_000f058c(Tree0E *root, Tree0E *n) {
    if (root != ((void *)0)) {
        if ((*(u16 *)((u8 *)(n) + (0x10))) == (*(u16 *)((u8 *)(root) + (0x10))) ? (*(f32 *)((u8 *)(n) + (8))) > (*(f32 *)((u8 *)(root) + (8)))
                                                           : (*(u16 *)((u8 *)(n) + (0x10))) < (*(u16 *)((u8 *)(root) + (0x10)))) {
            root->left = func_000f058c(root->left, n);
        } else {
            root->right = func_000f058c(root->right, n);
        }
    } else {
        root = n;
    }
    return root;
}
