#include "text_metrics.h"
int lordly_text_lead_byte(uint32_t character) {
    const uint8_t c = (uint8_t)character;
    return (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc);
}
int lordly_text_glyph_width(uint32_t character, int alternate, int16_t fixed_index,
                            const uint8_t *normal, size_t normal_size, const uint8_t *other,
                            size_t other_size, uint8_t *width) {
    const uint8_t c = (uint8_t)character;
    int index;
    const uint8_t *table;
    size_t size;
    if (c < 17) {
        *width = 0;
        return 0;
    }
    if (lordly_text_lead_byte(c)) {
        *width = 11;
        return 0;
    }
    index = fixed_index != -1 ? fixed_index : (int)c + (c < 128 ? 16 : 0);
    table = alternate ? other : normal;
    size = alternate ? other_size : normal_size;
    if (!table || index < 0 || (size_t)index >= size)
        return -1;
    *width = table[index];
    return 0;
}
