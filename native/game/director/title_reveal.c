/* Portable reconstruction of title transition ROM290C50..291130.
 * DMA setup ROM2829BC..2829D8 loads ROM286BD0..29A4C0 at live8022AC90.
 * Constructor/update/render/query are live80234D10/4DB8/4E10/519C.
 * Unlike the main-scene crop, -1/-1 is not a preset. */
#include "title_reveal.h"
static int32_t half(const uint8_t *p) {
    const uint32_t v = (uint32_t)p[0] << 8 | p[1];
    return v < 32768 ? (int32_t)v : (int32_t)v - 65536;
}
static void put(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
void lordly_title_start(uint8_t r[14], const uint32_t a[6]) {
    put(r, a[2]);
    put(r + 2, a[3]);
    put(r + 4, a[0]);
    put(r + 6, a[1]);
    put(r + 8, 0);
    put(r + 10, a[4]);
    put(r + 12, a[5]);
}
int lordly_title_update(uint8_t r[14]) {
    const int32_t progress = half(r + 8);
    if (progress == half(r + 10))
        return half(r + 12) != 0;
    put(r + 8, (uint32_t)(progress + 1));
    return 1;
}
int lordly_title_rectangles(const uint8_t r[14], int high, uint32_t w[8]) {
    const int32_t duration = half(r + 10), progress = half(r + 8);
    int32_t edges[2];
    int i;
    if (!duration)
        return 0;
    for (i = 0; i < 2; ++i) {
        const int32_t initial = half(r + 4 + 2 * i), final = half(r + 2 * i);
        edges[i] = initial + (final - initial) * progress / duration;
    }
    for (i = 0; i < 8; ++i)
        w[i] = 0;
    if (!high) {
        w[0] = 0xe44fc000U | ((uint32_t)edges[0] * 4 & 0xfffU);
        w[4] = 0xe45003c0U;
        w[5] = (uint32_t)edges[1] * 4 & 0xfffU;
    } else {
        uint32_t y[2];
        int32_t signedQuarter[2];
        for (i = 0; i < 2; ++i) {
            const uint32_t quarter = (uint32_t)edges[i] * 4;
            const uint32_t low = quarter & 0xffffU;
            signedQuarter[i] = low < 32768 ? (int32_t)low : (int32_t)low - 65536;
            y[i] = signedQuarter[i] < 0 ? 0 : quarter & 0xffcU;
        }
        w[0] = 0xe4a00000U | y[0];
        w[4] = 0xe4a00780U;
        w[5] = y[1];
        /* Original branch checks the full shifted word, then uses its signed
         * halfword to compensate the texture origin when clipping. */
        if (((uint32_t)edges[1] * 4 & 0x80000000U) && signedQuarter[1] < 0)
            w[6] = (uint32_t)(-signedQuarter[1] * 8) & 0xffffU;
        w[3] = w[7] = 0x04000400U;
    }
    return 1;
}
