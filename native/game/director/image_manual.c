/* Host reconstruction of Rev0 ROM2B4104, uniquely matched in saved RAM at
 *8023F304. Native analog division truncates before movement; boundaries are
 *checked before applying both input sources. No post-movement clamp. */
#include "image_motion.h"
#include <math.h>
#include <string.h>
static uint32_t word(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static void put(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
static float real(const uint8_t *p) {
    uint32_t v = word(p);
    float f;
    memcpy(&f, &v, 4);
    return f;
}
static void put_real(uint8_t *p, float f) {
    uint32_t v;
    memcpy(&v, &f, 4);
    put(p, v);
}
static int half(const uint8_t *p) {
    const int v = (int)p[0] * 256 + p[1];
    return v >= 32768 ? v - 65536 : v;
}
int lordly_image_manual(uint8_t c[0x848], uint8_t channel[88], LordlyImageInput input,
                        const double constants[4]) {
    if (c[0x83f] != 1)
        return 1;
    float x = real(channel + 0x40), y = real(channel + 0x44), scale = real(channel + 0x54);
    const int width = half(c + 0x834), height = half(c + 0x836);
    unsigned blocked = 0;
    uint8_t available = 255;
    if (!isfinite(x) || !isfinite(y) || !isfinite(scale))
        return 0;
    if (width < 281) {
        blocked = 3;
        available &= 0x42;
    } else if (x > (float)((width - 280) / 2)) {
        blocked = 1;
        available &= 0xd6;
    } else if (x < (float)((280 - width) / 2)) {
        blocked = 2;
        available &= 0x6b;
    }
    if (height < 191) {
        blocked |= 12;
        available &= 0x18;
    } else if (y > (float)((height - 190) / 2)) {
        blocked |= 4;
        available &= 0xf8;
    } else if (y < (float)((190 - height) / 2)) {
        blocked |= 8;
        available &= 0x1f;
    }
    if (input.controller_present) {
        const float dx =
            input.stick_x >= -4 && input.stick_x <= 4 ? 0.f : (float)input.stick_x / 16.f;
        const float dy =
            input.stick_y >= -4 && input.stick_y <= 4 ? 0.f : (float)input.stick_y / 16.f;
        if ((dx > 0 && !(blocked & 1)) || (dx < 0 && !(blocked & 2)))
            x += (float)(int16_t)dx;
        if ((dy > 0 && !(blocked & 4)) || (dy < 0 && !(blocked & 8)))
            y += (float)(int16_t)dy;
    }
    if ((input.digital & 0x800) && !(blocked & 4))
        y += 2.f;
    if ((input.digital & 0x400) && !(blocked & 8))
        y -= 2.f;
    if ((input.digital & 0x100) && !(blocked & 1))
        x += 2.f;
    if ((input.digital & 0x200) && !(blocked & 2))
        x -= 2.f;
    LordlyImageZoom zoom = {c[0x83c], word(c + 0x828), word(c + 0x830)};
    if ((input.digital & 8) && zoom.state == 3) {
        zoom.state = 2;
        zoom.elapsed = 0;
        zoom.duration = 30;
    }
    if ((input.digital & 4) && zoom.state == 4) {
        zoom.state = 1;
        zoom.elapsed = 0;
        zoom.duration = 30;
    }
    if (!lordly_image_zoom_advance(&zoom, &scale, constants) || !isfinite(x) || !isfinite(y))
        return 0;
    c[0x83e] = available;
    c[0x83c] = zoom.state;
    put(c + 0x828, zoom.elapsed);
    put(c + 0x830, zoom.duration);
    put_real(channel + 0x40, x);
    put_real(channel + 0x44, y);
    put_real(channel + 0x54, scale);
    return 1;
}
