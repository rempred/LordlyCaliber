#include "actor_render.h"
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
