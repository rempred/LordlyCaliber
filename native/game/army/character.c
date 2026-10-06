#include "character.h"
int lordly_character_level_one(uint8_t r[56], const uint8_t *classes, unsigned count,
                               LordlyCharacterRandom random, void *context) {
    const uint8_t *selected, *base;
    unsigned primary = r[17], secondary = r[18], growth, i;
    static const uint8_t from[] = {8, 12, 16, 20, 24, 28, 32};
    static const uint8_t to[] = {22, 28, 30, 32, 34, 36, 38};
    if (!classes || primary >= count || secondary >= count || !random)
        return 0;
    selected = classes + 72 * primary;
    if (selected[69] != secondary)
        selected = classes + 72 * secondary;
    growth = (unsigned)(selected - classes) / 72;
    if (selected[66] != 255 && selected[66] > 1)
        growth = selected[65];
    else if (selected[68] != 255 && selected[68] > 1)
        growth = selected[67];
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
    r[24] = r[22];
    r[25] = r[23];
    r[40] = selected[35];
    return 1;
}
