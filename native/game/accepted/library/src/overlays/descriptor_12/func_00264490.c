#include "game/effect_geometry_e05_internal.h"

extern u8 *D_801969B0;
#define E05_EMIT(a, b) COMBAT_DRAW_PAIR((a), (u32)(b))

s32 func_00264490(E05OuterObject2641B0 *input)
{
    E05Object2641B0 *object = &input->object_18;
    func_002639E0((struct E05DrawObject *)(object));
    E05_EMIT(0xFA000000, (u32)(input->object_18.transform_04->value_40 * 255.0f) & 0xFF);
    func_800988A0(object->transform_04, D_801969B0);
    E05_EMIT(0xDA380000, D_801969B0);
    if (object->segment_0C) {
        E05_EMIT(0xE7000000, 0);
        E05_EMIT(0xDB06003C, (u32)object->segment_0C - 0x80000000);
        E05_EMIT(0xE7000000, 0);
        E05_EMIT(0xDE000000, object->list_08);
    }
    E05_EMIT(0xE7000000, 0);
    E05_EMIT(0xDB060038, (u32)object->segment_14 - 0x80000000);
    E05_EMIT(0xE7000000, 0);
    E05_EMIT(0xDE000000, 0x0E000000);
    if (object->list_18) {
        E05_EMIT(0xE7000000, 0);
        E05_EMIT(0xDB06003C, (u32)object->list_18 - 0x80000000);
        E05_EMIT(0xE7000000, 0);
        E05_EMIT(0xDE000000, object->list_08);
        E05_EMIT(0xE7000000, 0);
        E05_EMIT(0xDB060038, (u32)object->segment_14 - 0x80000000);
        E05_EMIT(0xE7000000, 0);
        E05_EMIT(0xDE000000, 0x0E000000);
    }
    E05_EMIT(0xD8380002, 0x40);
    D_801969B0 += 0x40;
    return 0;
}
