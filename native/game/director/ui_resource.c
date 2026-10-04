#include "ui_resource.h"
static uint16_t half(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] * 256 + p[1]); }
static int signed_half(uint32_t v) { v &= 65535U; return v < 32768U ? (int)v : (int)v - 65536; }
static void put_half(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void put_word(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}
int lordly_ui_register(LordlyUiPool *pool, uint16_t flags, uint32_t initializer,
                      uint16_t x, uint16_t y, uint16_t right, uint16_t bottom,
                      uint16_t priority) {
    int slot;
    uint8_t *p;
    if (!pool) return -1;
    for (slot = 0; slot < LORDLY_UI_SLOTS; ++slot)
        if (!(half(pool->records[slot].bytes) & 0x8000)) break;
    if (slot == LORDLY_UI_SLOTS) return -1;
    p = pool->records[slot].bytes;
    put_half(p, flags); put_word(p + 0x10, initializer);
    put_half(p + 6, x); put_half(p + 8, y);
    put_half(p + 10, right); put_half(p + 12, bottom); put_half(p + 14, priority);
    put_half(p + 0xa2, (uint32_t)pool->context); put_half(p + 0xa4, pool->parent);
    return slot;
}
int lordly_ui_window(LordlyUiPool *pool, uint32_t initializer, uint16_t x, uint16_t y,
                    uint16_t kind, uint16_t style) {
    const int slot = lordly_ui_register(pool, 0xc800, initializer, x, y, x, y, 0xf000);
    uint8_t *p;
    uint32_t edge;
    if (slot < 0) return slot;
    p = pool->records[slot].bytes;
    edge = (uint32_t)half(p + 6) + (uint32_t)pool->margin - 1U;
    put_half(p + 10, edge);
    if (signed_half(edge) >= 320) {
        put_half(p + 10, 319); put_half(p + 6, 320U - (uint32_t)pool->margin);
    }
    if (signed_half(half(p + 6)) < 0) put_half(p + 6, 0);
    edge = (uint32_t)half(p + 8) + (uint32_t)pool->margin - 1U;
    put_half(p + 12, edge);
    if (signed_half(edge) >= 240) {
        put_half(p + 12, 239); put_half(p + 8, 240U - (uint32_t)pool->margin);
    }
    if (signed_half(half(p + 8)) < 0) put_half(p + 8, 0);
    if (signed_half(half(p + 8)) >= 128) {
        p[2] |= 0x10;
        put_half(p + 8, (uint32_t)half(p + 8) - 28U);
        put_half(p + 12, (uint32_t)half(p + 12) - 28U);
    }
    put_half(p + 0x28, kind); put_half(p + 0x2a, style);
    return slot;
}
