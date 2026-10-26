/* Screen-centred RGBA5551 effect frames. The texture-rectangle state follows
 * the carried accepted func_000E6D90/dialogue_draw_plan drawing contract.
 * Tiles stay within 4 KiB TMEM. Inputs and output extent are bounded. */
typedef unsigned int EffectWord;
#define EFFECT_PAIR(a, b)                                                                          \
    do {                                                                                           \
        if (count + 2 > capacity)                                                                  \
            return -1;                                                                             \
        out[count++] = (EffectWord)(a);                                                            \
        out[count++] = (EffectWord)(b);                                                            \
    } while (0)
int lc_visual_image_commands(EffectWord *out, EffectWord pixels, int width, int height, int anchorX,
                             int anchorY, int capacity) {
    int count = 0, x, y, w, h, left, top, right, bottom, s, t, stride, line;
    if (width < 1 || height < 1 || width > 256 || height > 256 || capacity < 16)
        return -1;
    stride = (width + 3) & ~3;
    EFFECT_PAIR(0xe7000000, 0);
    EFFECT_PAIR(0xe3000a01, 0);
    EFFECT_PAIR(0xe3001001, 0);
    EFFECT_PAIR(0xe3001201, 0);
    EFFECT_PAIR(0xe200001c, 0x00504240);
    EFFECT_PAIR(0xfcffffff, 0xfffffe79);
    for (y = 0; y < height; y += 32)
        for (x = 0; x < width; x += 32) {
            w = width - x;
            if (w > 32)
                w = 32;
            h = height - y;
            if (h > 32)
                h = 32;
            left = 160 - anchorX + x;
            top = 120 - anchorY + y;
            right = left + w;
            bottom = top + h;
            if (right <= 0 || bottom <= 0 || left >= 320 || top >= 240)
                continue;
            s = x * 32;
            t = y * 32;
            if (left < 0) {
                s -= left * 32;
                left = 0;
            }
            if (top < 0) {
                t -= top * 32;
                top = 0;
            }
            if (right > 320)
                right = 320;
            if (bottom > 240)
                bottom = 240;
            line = (w * 2 + 7) >> 3;
            EFFECT_PAIR(0xe7000000, 0);
            EFFECT_PAIR(0xfd100000 | (stride - 1), pixels);
            EFFECT_PAIR(0xf5100000 | (line << 9), 0x07000000);
            EFFECT_PAIR(0xe6000000, 0);
            EFFECT_PAIR(0xf4000000 | ((x * 4) << 12) | (y * 4),
                        0x07000000 | (((x + w - 1) * 4) << 12) | ((y + h - 1) * 4));
            EFFECT_PAIR(0xe7000000, 0);
            EFFECT_PAIR(0xf5100000 | (line << 9), 0x00080200);
            EFFECT_PAIR(0xf2000000 | ((x * 4) << 12) | (y * 4),
                        (((x + w - 1) * 4) << 12) | ((y + h - 1) * 4));
            EFFECT_PAIR(0xe4000000 | ((right * 4) << 12) | (bottom * 4),
                        ((left * 4) << 12) | (top * 4));
            EFFECT_PAIR(0xe1000000, ((s & 65535) << 16) | (t & 65535));
            EFFECT_PAIR(0xf1000000, 0x04000400);
        }
    return count;
}
#undef EFFECT_PAIR
