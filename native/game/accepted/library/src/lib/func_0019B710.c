#include "common/types.h"

extern u16 *D_800C4C4C;
extern void func_8020CDA0(s32 value);

void func_0019B710(u8 *value, u8 maximum)
{
    u16 flags = *D_800C4C4C;
    u8 result;
    if (flags & 0x200) {
        result = *value | 0x80;
    } else if (flags & 0x100) {
        result = *value & 0x7F;
    } else {
        u8 current;
        s32 updated;
        if (!(flags & 0xC00)) {
            return;
        }
        current = *value;
        updated = current & 0x7F;
        /* Keep explicit arms and byte consumers: KMC merges or removes
         * conversions when these updates are expressed as shared temporaries. */
        if (flags & 0x800) {
            s32 increased;
            if (current & 0x80) {
                increased = updated + 10;
            } else {
                increased = (u8)updated + 1;
            }
            maximum &= 0xFF;
            if (maximum < increased) {
                increased = maximum;
            }
            updated = increased;
        } else {
            s32 decreased;
            if (current & 0x80) {
                decreased = (u8)updated - 10;
            } else {
                decreased = (u8)updated - 1;
            }
            if (decreased <= 0) {
                decreased = 1;
            }
            updated = decreased;
        }
        if ((u8)updated != (*value & 0x7F)) {
            func_8020CDA0(2);
        }
        result = updated + (*value & 0x80);
    }
    *value = result;
}
