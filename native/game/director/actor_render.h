#ifndef LORDLY_ACTOR_RENDER_H
#define LORDLY_ACTOR_RENDER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int lordly_actor_primitive(uint8_t rgba[4], const uint8_t actor[336], int layer, uint8_t alpha,
                           uint8_t shadow, const int16_t global_rgb[3]);
int lordly_actor_render_class_size(const uint8_t *classes, int count, int source, int owner,
                                   uint8_t *size);
#ifdef __cplusplus
}
#endif
#endif
