#ifndef LORDLY_TEXT_METRICS_H
#define LORDLY_TEXT_METRICS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ROM 51450 and 4F2E4, in the qualified cold-boot resident slab. */
int lordly_text_lead_byte(uint32_t character);
int lordly_text_glyph_width(uint32_t character, int alternate, int16_t fixed_index,
                            const uint8_t *normal, size_t normal_size, const uint8_t *other,
                            size_t other_size, uint8_t *width);
#ifdef __cplusplus
}
#endif
#endif
