#include "character.h"
/* PC adaptation of accepted func_0004501C and func_00044aa4. Explicit
 * big-endian records replace N64 structs; callers supply tables and RNG.
 * The matching originals and equipment getters are in game/accepted. */
static unsigned be16(const uint8_t *p) { return ((unsigned)p[0] << 8) | p[1]; }
static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static unsigned growth_class(const uint8_t *selected, unsigned id, unsigned level) {
    if (selected[66] != 255 && level < selected[66]) return selected[65];
    if (selected[68] != 255 && level < selected[68]) return selected[67];
    return id;
}
static int level_up(uint8_t *r, const uint8_t *classes, const uint8_t *items,
                    unsigned item_count, LordlyCharacterRandom random, void *context) {
    unsigned i, slot, id, growth[7], luck_delta = 0;
    static const uint8_t to[] = {22, 28, 30, 32, 34, 36, 38};
    const uint8_t *base = classes + 72 * r[18];
    const uint8_t *equipment = classes + 72 * r[17];
    if (equipment[69] != r[18]) equipment = base;
    for (i = 0; i < 7; ++i) {
        unsigned extra = 1;
        if (r[18] != 1) {
            extra = random(context) % 2;
            extra += random(context) % 2;
        }
        growth[i] = base[10 + 4 * i] + extra;
    }
    for (slot = 0; slot < 4; ++slot) {
        const uint8_t *item;
        id = be16(r + 42 + 2 * slot);
        if (!id) id = be16(equipment + 46 + 2 * slot);
        if (!id) continue;
        if (!items || id >= item_count) return 0;
        item = items + 32 * id;
        /* Retail deliberately uses 457E4 for both HP and STR growth. */
        growth[0] += item[24] >> 6;
        growth[1] += item[24] >> 6;
        growth[2] += (item[25] >> 4) & 3;
        growth[3] += (item[24] >> 2) & 3;
        growth[4] += (item[25] >> 2) & 3;
        growth[5] += item[24] & 3;
        growth[6] += item[25] >> 6;
        luck_delta += item[25] & 3;
    }
    for (i = 0; i < 7; ++i) {
        unsigned value = be16(r + to[i]) + growth[i];
        put16(r + to[i], value < 1000 ? value : 999);
    }
    r[40] = (uint8_t)(r[40] + luck_delta > 100 ? 100 : r[40] + luck_delta);
    ++r[19];
    return 1;
}
int lordly_character_initialize(uint8_t r[56], unsigned level, const uint8_t *classes,
                                unsigned count, const uint8_t *items, unsigned item_count,
                                LordlyCharacterRandom random, void *context) {
    const uint8_t *selected, *base;
    unsigned primary = r[17], secondary = r[18], growth, i;
    static const uint8_t from[] = {8, 12, 16, 20, 24, 28, 32};
    static const uint8_t to[] = {22, 28, 30, 32, 34, 36, 38};
    if (!classes || primary >= count || secondary >= count || !random || level < 1 || level > 99)
        return 0;
    selected = classes + 72 * primary;
    if (selected[69] != secondary)
        selected = classes + 72 * secondary;
    growth = (unsigned)(selected - classes) / 72;
    growth = growth_class(selected, growth, 1);
    if (growth >= count)
        return 0;
    base = classes + 72 * growth;
    r[20] = base[5] == 3 ? 0 : base[5] == 4 ? (uint8_t)(random(context) & 1) : base[5];
    for (i = 0; i < 7; ++i) {
        r[to[i]] = base[from[i]];
        r[to[i] + 1] = base[from[i] + 1];
    }
    r[26] = selected[70] == 255 ? (uint8_t)(random(context) % 4 + 1) : selected[70];
    r[27] = selected[36];
    r[19] = 1;
    for (i = 1; i < level; ++i) {
        growth = growth_class(selected, (unsigned)(selected - classes) / 72, i);
        if (growth >= count) return 0;
        r[17] = r[18] = (uint8_t)growth;
        if (!level_up(r, classes, items, item_count, random, context)) return 0;
    }
    r[17] = (uint8_t)primary;
    r[18] = (uint8_t)secondary;
    r[24] = r[22];
    r[25] = r[23];
    r[40] = selected[35];
    return 1;
}
int lordly_character_level_one(uint8_t r[56], const uint8_t *classes, unsigned count,
                               LordlyCharacterRandom random, void *context) {
    return lordly_character_initialize(r, 1, classes, count, 0, 0, random, context);
}
