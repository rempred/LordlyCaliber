/* Screen-centred RGBA5551/RGBA32 effect frames. The texture-rectangle state follows
 * the carried accepted func_000E6D90/dialogue_draw_plan drawing contract.
 * Tiles stay within 4 KiB TMEM. Inputs and output extent are bounded. */
typedef unsigned int EffectWord;
/* Camera matrices are established by accepted func_0025F800: world -> view ->
 * perspective, row-vector convention. Return only finite, in-front positions. */
static int effect_project(const float *point, const float *view, const float *projection,
                          int *out) {
    float p[4], q[4], x, y;
    int i, j;
    for (i = 0; i < 4; ++i) {
        p[i] = view[12 + i];
        for (j = 0; j < 3; ++j)
            p[i] += point[j] * view[j * 4 + i];
    }
    for (i = 0; i < 4; ++i) {
        q[i] = 0;
        for (j = 0; j < 4; ++j)
            q[i] += p[j] * projection[j * 4 + i];
    }
    if (!(q[3] > 0.0001f))
        return 0;
    x = 160.0f + q[0] / q[3] * 160.0f;
    y = 120.0f - q[1] / q[3] * 120.0f;
    if (!(x > -4096 && x < 4096 && y > -4096 && y < 4096))
        return 0;
    out[0] = (int)x;
    out[1] = (int)y;
    return 1;
}
/* Actual selected actors and side flags. The native actor-presence predicate
 * marks occupied slots; dead/revival targets are deliberately retained. */
int lc_visual_origins(int placement, const unsigned char *source, const unsigned char *targets,
                      const float *positions, EffectWord occupied, EffectWord opponents,
                      const float *view, const float *projection, int *out) {
    int i, j, actor, count = 0, side, n = 0;
    EffectWord seen = 0;
    float centre[3] = {0, 0, 0};
    if (!placement) {
        out[0] = 160;
        out[1] = 120;
        return 1;
    }
    if (placement == 2)
        return source && source[0] < 20 && (occupied & (1U << source[0]))
                   ? effect_project(positions + source[0] * 3, view, projection, out)
                   : 0;
    if (!targets)
        return 0;
    if (placement == 3) {
        if (targets[0] >= 20)
            return 0;
        side = (opponents >> targets[0]) & 1;
        for (i = 0; i < 20; ++i)
            if ((occupied & (1U << i)) && ((opponents >> i) & 1) == side) {
                for (j = 0; j < 3; ++j)
                    centre[j] += positions[i * 3 + j];
                ++n;
            }
        if (!n)
            return 0;
        for (j = 0; j < 3; ++j)
            centre[j] /= n;
        return effect_project(centre, view, projection, out);
    }
    for (i = 0; i < 20 && targets[i] != 255; ++i) {
        actor = targets[i];
        if (actor >= 20 || !(occupied & (1U << actor)) || (seen & (1U << actor)))
            continue;
        seen |= 1U << actor;
        count += effect_project(positions + actor * 3, view, projection, out + count * 2);
    }
    return count;
}
#define EFFECT_PAIR(a, b)                                                                          \
    do {                                                                                           \
        if (count + 2 > capacity)                                                                  \
            return -1;                                                                             \
        out[count++] = (EffectWord)(a);                                                            \
        out[count++] = (EffectWord)(b);                                                            \
    } while (0)
int lc_visual_image_commands_format(EffectWord *out, EffectWord pixels, int width, int height, int anchorX,
                             int anchorY, int capacity, int rgba32) {
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
            EFFECT_PAIR((rgba32 ? 0xfd180000 : 0xfd100000) | (stride - 1), pixels);
            EFFECT_PAIR((rgba32 ? 0xf5180000 : 0xf5100000) | (line << 9), 0x07000000);
            EFFECT_PAIR(0xe6000000, 0);
            EFFECT_PAIR(0xf4000000 | ((x * 4) << 12) | (y * 4),
                        0x07000000 | (((x + w - 1) * 4) << 12) | ((y + h - 1) * 4));
            EFFECT_PAIR(0xe7000000, 0);
            EFFECT_PAIR((rgba32 ? 0xf5180000 : 0xf5100000) | (line << 9), 0x00080200);
            EFFECT_PAIR(0xf2000000 | ((x * 4) << 12) | (y * 4),
                        (((x + w - 1) * 4) << 12) | ((y + h - 1) * 4));
            EFFECT_PAIR(0xe4000000 | ((right * 4) << 12) | (bottom * 4),
                        ((left * 4) << 12) | (top * 4));
            EFFECT_PAIR(0xe1000000, ((s & 65535) << 16) | (t & 65535));
            EFFECT_PAIR(0xf1000000, 0x04000400);
        }
    return count;
}
/* 32-bit textures split their RG and BA halves across the two TMEM banks;
 * the tile line field therefore remains the 16-bit width. A 32x32 tile uses
 * 2048 bytes in each bank, within the 4KiB total. */
int lc_visual_image_commands(EffectWord *out, EffectWord pixels, int width, int height, int anchorX,
                             int anchorY, int capacity) {
    return lc_visual_image_commands_format(out, pixels, width, height, anchorX, anchorY, capacity, 0);
}
#undef EFFECT_PAIR
