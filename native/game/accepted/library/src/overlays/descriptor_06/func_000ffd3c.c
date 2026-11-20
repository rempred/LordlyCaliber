typedef unsigned char u8;
typedef signed short s16;
typedef unsigned int u32;
void func_000ffd3c(u8 *o) {
    u32 i;
    if ((*(s16 *)((u8 *)(o) + (0x88))) < (*(s16 *)((u8 *)(o) + (0x8A)))) {
        for (i = 0; i < 4; i++) {
            ((s16 *)(o + 0x8C))[i] = ((s16 *)(o + 0x94))[i] +
                (((s16 *)(o + 0x9C))[i] - ((s16 *)(o + 0x94))[i]) * (*(s16 *)((u8 *)(o) + (0x88))) / (*(s16 *)((u8 *)(o) + (0x8A)));
        }
        (*(s16 *)((u8 *)(o) + (0x88)))++;
    } else {
        for (i = 0; i < 4; i++) {
            ((s16 *)(o + 0x8C))[i] = ((s16 *)(o + 0x9C))[i];
        }
    }
}
