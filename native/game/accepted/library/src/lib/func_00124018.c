typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;

extern u8 D_801971F0[][25];
extern u16 D_8019532C[];
extern RuntimeUnit *D_801F0CB0[];
extern u32 func_0014F300(s32 source_word);
extern void func_00131388(void *record);
extern s32 func_00131050();
extern void func_00147FB8(int unused_first, int source_word);
extern void func_0015DF10(u32 value);

s32 func_00124018(s32 source_word, s32 selector, s32 member)
{
    /* This default also supplies the retail setup before the prologue. */
    f32 ratio = 0.25f;
    s32 eligible;
    s32 index;
    s32 count;
    u8 *record;

    if (selector == 1)
        ratio = 0.35f;
    eligible = 0;
    if (member < 100) {
        u8 *row = (u8 *)0x80190000 + member * 52;
        s32 current = *(u16 *)(row + 0x5578);
        if (current != 0) {
            f32 reference = (f32)*(u16 *)(row + 0x5576);
            if ((f32)current < ratio * reference)
                eligible = 1;
        }
    }

    if (eligible != 0) {
        count = (u16)func_0014F300((u8)source_word);
        record = D_801971F0[source_word];
        for (index = 0; index < count; ++index) {
            u8 *entry = record + index;
            s32 value = entry[13];
            if (value == 1) {
                u8 *row = (u8 *)0x80190000 + member * 52;
                u32 next = *(u16 *)(row + 0x5578) + 100;
                u32 reference = *(u16 *)(row + 0x5576);
                /* Wrap to16 bits before comparing with the reference. */
                *(u16 *)(row + 0x5578) = next;
                if ((u16)next > reference)
                    *(u16 *)(row + 0x5578) = reference;
                entry[13] = 0;
                func_00131388(record);
                break;
            } else if (value == 2) {
                u8 *row = (u8 *)0x80190000 + member * 52;
                u32 next = *(u16 *)(row + 0x5578) + 300;
                u32 reference = *(u16 *)(row + 0x5576);
                *(u16 *)(row + 0x5578) = next;
                if ((u16)next > reference)
                    *(u16 *)(row + 0x5578) = reference;
                entry[13] = 0;
                func_00131388(record);
                break;
            } else if (value == 3) {
                s32 slot;
                entry[13] = 0;
                func_00131388(record);
                for (slot = 0; slot < 5; ++slot) {
                    member = (record + slot)[2];
                    if (member != 0) {
                        if (member < 100) {
                            u8 *row = (u8 *)0x80190000 + member * 52;
                            u32 current = *(u16 *)(row + 0x5578);
                            if (current != 0) {
                                u32 reference = *(u16 *)(row + 0x5576);
                                u32 next = current + 150;
                                *(u16 *)(row + 0x5578) = next;
                                if ((u16)next > reference)
                                    *(u16 *)(row + 0x5578) = reference;
                            }
                        } else {
                            u16 *field = &D_8019532C[member];
                            u32 current = *field;
                            if (current != 0) {
                                u32 next = current + 150;
                                u32 reference;
                                *field = next;
                                reference = D_8019532C[99];
                                if ((u16)next > reference)
                                    *field = reference;
                            }
                        }
                    }
                }
                break;
            }
        }
        if (index == count) {
            eligible = 0;
        } else {
            if ((*(u32 *)D_801F0CB0[source_word] & 0x20) != 0) {
                func_00131050();
                func_00147FB8(1, source_word);
                func_0015DF10(19);
            }
        }
    }
    return eligible;
}
