typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
void *func_00001330(s32 size);
void *func_000016C4(void *p);
void bzero(void *p, s32 n);
typedef struct {
    void *unk00;
    u8 *pixels;
    u8 *tex;
    void *chunk0C;
    void *sprite;
    void *chunk14;
} Res28;
extern Res28 *D_8023DE34;
extern u8 D_800EB0B0[];
void *D_8009DBB8(void *dst, u32 handle);
void *D_8009DD38(u32 handle);
void *D_8016FBE0(u8 cls, u8 variant, void *out);
void D_800EA6B0();
void bzero(void *p, s32 n);
void bzero(void *p, s32 n);
void func_0028ad2c(void) {
    u8 *raw;
    D_8023DE34 = func_00001330(0x68);
    bzero(D_8023DE34, 0x68);
    raw = D_8009DBB8(((void *)0), 0x1890C90);
    D_8023DE34->chunk14 = D_8009DD38(*(u32 *)(raw + 0x4C));
    func_000016C4(raw);
    D_8023DE34->sprite = D_8016FBE0(0x7D, 0, D_8023DE34);
    D_8023DE34->tex = func_00291130(&D_8023DE34->pixels);
    D_8023DE34->chunk0C = D_8009DD38(0x1885534);
    D_800EA6B0(D_800EB0B0, 7, 5);
}
u8 *func_00291130(u8 **out);
