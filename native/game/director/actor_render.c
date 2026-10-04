#include "actor_render.h"
#include <math.h>
/* ROM2ADF9C: the world draw driver applies this environment after the Actor's
 * own tint/material commands. The selected six-byte palette row is explicit. */
int lordly_actor_world_environment(uint8_t rgba[4], float x, float z, const uint8_t gradient[6]) {
    float position, remaining;
    int i;
    if (!isfinite(x) || !isfinite(z))
        return 0;
    position = x + 152.f;
    position = position + z;
    position = position + 38.f;
    if (!isfinite(position))
        return 0;
    if (position < 0.f)
        position = 0.f;
    else if (position > 380.f)
        position = 380.f;
    remaining = 380.f - position;
    for (i = 0; i < 3; ++i) {
        float delta = (float)gradient[i + 3] - (float)gradient[i];
        float color = remaining * delta;
        color = color / 380.f;
        color = color + (float)gradient[i];
        rgba[i] = (uint8_t)(uint32_t)color;
    }
    rgba[3] = 1;
    return 1;
}
/* Portable reconstruction of the primitive-color portion of ROM29EAFC..29EE18.
 * Original assembly Decomp62cc7ef4; not accepted matching C. */
int lordly_actor_primitive(uint8_t rgba[4], const uint8_t actor[336], int layer, uint8_t alpha,
                           uint8_t shadow, const int16_t global_rgb[3]) {
    int opacity = alpha, i;
    if (layer < 0 || layer >= 16)
        return 0;
    if (shadow == 1)
        opacity = opacity * actor[0x13c] / 255;
    opacity = opacity * actor[layer] / 255;
    for (i = 0; i < 3; ++i)
        rgba[i] = global_rgb[0] == 255 ? actor[0x142 + i]
                                       : (uint8_t)(actor[0x142 + i] * global_rgb[i] / 255);
    rgba[3] = (uint8_t)opacity;
    return 1;
}
/* Adapted accepted func_00043d1c.c, promotion43d43ddd. Host table/count replace
 * the target global pointer; explicit validation precedes the accepted choice. */
int lordly_actor_render_class_size(const uint8_t *classes, int count, int source, int owner,
                                   uint8_t *size) {
    source &= 255;
    owner &= 255;
    if (source >= count || owner >= count)
        return 0;
    *size = classes[(classes[source * 72 + 69] == owner ? source : owner) * 72 + 4];
    return 1;
}
