typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;

/* One RDP/RSP display-list command: two 32-bit words. */
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;

extern Gfx *D_800E9BA0;  /* current display-list write head */
extern void *D_8019E1AC; /* lazily allocated 0x780-byte clear texture and palette */
extern u8 D_8018F480;

extern void *func_00045bec(u32 identity, u32 variant, void **palette);
extern void *func_00070F30(s32 size);
extern void func_00023780(void *pointer, s32 size);

/*
 * Dialogue portrait texture loader.
 *
 * Resolves the portrait identity and variant to a 40-by-48 CI8 texture and its
 * palette. With no resolved texture it allocates one 0x780-byte buffer once,
 * zero-fills it, and uses it as both texture and palette, so the portrait area
 * draws fully transparent. Then it appends the texture-image, tile, load,
 * palette and other-mode commands to the current display list. Command word
 * meanings are structural observations, not verified semantics.
 */
void func_000ead04(s32 identity, s32 variant)
{
    void *palette;
    void *texture;
    Gfx *gfx;
    void *clear;
    void *buffer;

    /* The renderer passes a signed halfword; lookup consumes byte selectors. */
    texture = func_00045bec((u8)identity, (u8)variant, &palette);
    if (texture == 0) {
        clear = D_8019E1AC;
        if (clear == 0) {
            buffer = func_00070F30(0x780);
            D_8019E1AC = buffer;
            func_00023780(buffer, 0x780);
        }
        texture = palette = D_8019E1AC;
    }

    gfx = D_800E9BA0++; gfx->w0 = 0xFD500000; gfx->w1 = (u32)texture;
    gfx = D_800E9BA0++; gfx->w0 = 0xF5500000; gfx->w1 = 0x07000000;
    gfx = D_800E9BA0++; gfx->w0 = 0xE6000000; gfx->w1 = 0;
    gfx = D_800E9BA0++; gfx->w0 = 0xF3000000; gfx->w1 = 0x073BF19A;
    gfx = D_800E9BA0++; gfx->w0 = 0xE7000000; gfx->w1 = 0;
    gfx = D_800E9BA0++; gfx->w0 = 0xF5480A00; gfx->w1 = 0;
    gfx = D_800E9BA0++; gfx->w0 = 0xF2000000; gfx->w1 = 0x0009C0BC;
    gfx = D_800E9BA0++; gfx->w0 = 0xFD100000; gfx->w1 = (u32)palette;
    gfx = D_800E9BA0++; gfx->w0 = 0xE8000000; gfx->w1 = 0;
    gfx = D_800E9BA0++; gfx->w0 = 0xF5000100; gfx->w1 = 0x07000000;
    gfx = D_800E9BA0++; gfx->w0 = 0xE6000000; gfx->w1 = 0;
    gfx = D_800E9BA0++; gfx->w0 = 0xF0000000; gfx->w1 = 0x073FC000;
    gfx = D_800E9BA0++; gfx->w0 = 0xE7000000; gfx->w1 = 0;
    D_8018F480 = 2;
    gfx = D_800E9BA0++; gfx->w0 = 0xE3001001; gfx->w1 = 0x8000;
}
