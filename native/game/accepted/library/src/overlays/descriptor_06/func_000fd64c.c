typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
void func_000016C4(void *p);
void func_000016C4(void *p);
typedef struct {
    u16 state;
    u8 unk02[0x16];
    s16 t;
    s16 n;
    s16 cur[4];
    s16 from[4];
    s16 to[4];
    u8 unk34[4];
    void *bufA;
    void *bufB;
} TweenFA5;
void func_000fd64c(TweenFA5 *tw) {
    s32 i;
    switch (tw->state) {
        case 0xFFFF:
            tw->state = 0xFFFE;
            return;
        case 0xFFFE:
            tw->state = 0xFFFD;
            return;
        case 0xFFFD:
            func_000016C4(tw->bufA);
            func_000016C4(tw->bufB);
            tw->state = 0;
            return;
        case 0:
            return;
        default:
            if (tw->t < tw->n) {
                for (i = 0; i < 4; i++) {
                    tw->cur[i] = tw->from[i] + (tw->to[i] - tw->from[i]) * tw->t / tw->n;
                }
                tw->t++;
            } else {
                for (i = 0; i < 4; i++) {
                    tw->cur[i] = tw->to[i];
                }
            }
            break;
    }
}
