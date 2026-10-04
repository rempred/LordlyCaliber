#include "actor_body.h"
static uint32_t word(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
/* ROM 20C120 predicate plus the initializer's primary/secondary distinction. */
int lordly_body_classification(const uint8_t *row, uint8_t secondary) {
    uint32_t owner;
    if (!row || !word(row + 0x48)) return 0;
    owner = word(row + 0x4c);
    if (owner != 10 && owner != 25 && owner != 123) return 0;
    return secondary == 1 ? 2 : 1;
}
/* Adapted accepted func_00043e88/43edc/43f30/43f84 (4123be2b).
 * ClassEntry's pointer begins 44 bytes into the complete class record. */
int lordly_body_appearance(const uint8_t *classes, int count, int source, int owner,
                           uint16_t appearance[4]) {
    int selected, i;
    source &= 255; owner &= 255;
    if (!classes || !appearance || count < 1 || source >= count || owner >= count) return 1;
    selected = classes[source * 72 + 69] == owner ? source : owner;
    for (i = 0; i < 4; ++i) {
        const uint8_t *p = classes + selected * 72 + 46 + i * 2;
        appearance[i] = (uint16_t)((uint16_t)p[0] * 256 + p[1]);
    }
    return 0;
}
/* Portable reconstruction of ROM 45934..45A50. */
int lordly_body_equipment(const uint8_t *items, int count, const uint16_t appearance[4],
                          uint16_t *equipment) {
    int i;
    if (!items || !appearance || !equipment || count < 1) return 1;
    for (i = 0; i < 4; ++i) {
        uint8_t type;
        if (appearance[i] >= count) return 1;
        type = items[appearance[i] * 32 + 16];
        if ((type > 0 && type < 14) || type == 24) {
            *equipment = appearance[i]; return 0;
        }
    }
    *equipment = 0; return 0;
}
/* Accepted func_00207658 (469a1416), shared by registration and allocation. */
int lordly_body_equipment_variant(int source, int equipment) {
    switch (source) {
    case 0x3b: return 11; case 0x3c: return 13; case 0x3d: return 15; case 0x3e: return 18;
    case 0x39: case 0x3f: case 0x40: return 2;
    case 0x41: return 3; case 0x42: return 4; case 0x43: return 5; case 0x44: return 6;
    case 0x3a: case 0x46: return 8;
    }
    return equipment;
}
