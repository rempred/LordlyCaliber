/* Battle caption integration. Native setup/frame/palette drawing is retained.
 * The native frame callback uses a pre-rendered label, not the supplied text.
 * Keep the action identity per side so identical names may have independent art. */
typedef unsigned char u8;
typedef unsigned int u32;
extern u8 lc_caption_data[];
extern u32 lc_caption_state[]; /* pending ID+1, active ID+1, two node/ID pairs */
extern void *D_801CE8BC;
extern void lc_original_caption_setup(const char *, int);
extern void lc_original_caption_draw(void *);
extern void *lc_original_caption_image(int);
extern void func_001F2BC0(const char *, int);
extern const char *func_00044340(int);
static int equal(const char *a, const char *b) {
    int i;
    for (i = 0; i < 129; i++) {
        if (a[i] != b[i])
            return 0;
        if (!a[i])
            return 1;
    }
    return 0;
}
static int identify(const char *name) {
    int i;
    u32 count = *(u32 *)(lc_caption_data + 12);
    for (i = 1; i < (int)count; i++)
        if (equal(name, func_00044340(i)))
            return i + 1;
    return 0;
}
void lc_attack_caption_name(int id, int side) {
    lc_caption_state[0] = id + 1;
    func_001F2BC0(func_00044340(id), side);
    lc_caption_state[0] = 0;
}
void lc_caption_setup(const char *name, int side) {
    char bounded[64];
    int i;
    u32 slot = side ? 2 : 4;
    u32 identity = lc_caption_state[0] ? lc_caption_state[0] : identify(name);
    /* Native per-side text buffers are 64 bytes; the editor permits 128-byte
     * names. Artwork retains the full name and identity even when this legacy
     * lookup string must be truncated. */
    for (i = 0; i < 63 && name[i]; ++i)
        bounded[i] = name[i];
    bounded[i] = 0;
    lc_original_caption_setup(bounded, side);
    lc_caption_state[slot] = *(u32 *)((u8 *)D_801CE8BC + (side ? 0x5748 : 0x574c));
    /* An expired opposite-side window can be recycled at the same address. */
    if (lc_caption_state[side ? 4 : 2] == lc_caption_state[slot])
        lc_caption_state[side ? 4 : 2] = 0;
    lc_caption_state[slot + 1] = identity;
}
void lc_caption_draw(void *node) {
    u32 previous = lc_caption_state[1];
    lc_caption_state[1] = (u32)node == lc_caption_state[2]   ? lc_caption_state[3]
                          : (u32)node == lc_caption_state[4] ? lc_caption_state[5]
                                                             : 0;
    lc_original_caption_draw(node);
    lc_caption_state[1] = previous;
}
void *lc_caption_image(int slot) {
    u32 id = lc_caption_state[1];
    if (id && id <= *(u32 *)(lc_caption_data + 12) && *(u32 *)(lc_caption_data + 8) >= 7) {
        u32 *table = *(u32 **)(lc_caption_data + 20);
        if (table && table[id - 1])
            return (void *)table[id - 1];
    }
    return lc_original_caption_image(slot);
}

/* The native caption scan can find a different action sharing this name.
 * Palette selection must follow the actual queued action, just like its art. */
extern unsigned char lc_original_caption_element(int,int,int,int,unsigned short);
unsigned char lc_caption_element(int id,int a,int b,int c,unsigned short d) {
    if(lc_caption_state[1]) id=(int)lc_caption_state[1]-1;
    return lc_original_caption_element(id,a,b,c,d);
}
