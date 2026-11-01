#include "game/effect_adapters_e08.h"
#include "game/effect_presentation_p04.h"

typedef struct P01Vec3 {
    float x, y, z;
} P01Vec3;


void func_0024E490(u32 value, const u8 *records, u32 count)
{
    P01Vec3 position;
    /* Keep the consumed byte count while retaining the word interface. */
    u8 countBits = (u8)count;
    s16 index;

    /* KMC marks const-pointer loads invariant; use an unqualified byte view
       to retain the load's dependencies on generated stack saves. */
    func_0024BF04((u8)value, &position, ((u8 *)records)[2] & 0xF0);
    for (index = 0; index < countBits; index++) {
        const u8 *record = records;

        records += 6;
        func_0024D180(&position, (void *)record);
    }
}
