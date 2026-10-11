typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;
typedef struct MovementAux MovementAux;
#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))
extern u8 D_800E7AB9, D_800E7A90[], D_801F0D98[];
extern s32 D_801F0DA8, D_801F0DAC;
extern u8 *D_801F0DB0;
extern u16 *D_801F0DB4, D_801F36D8[];
extern RuntimeUnit *D_801F0CB0[];
extern void *resource_alloc(u32 bytes);
extern void resource_free(void *object);
extern void func_00126D24(RuntimeUnit *unit);
extern s32 func_0012E8EC(RuntimeUnit *unit);
extern void func_00106CE0(f32 *first, f32 *second, f32 step);
extern s32 func_00107E38(MovementAux *aux);
extern void func_001072B8(RuntimeUnit *unit);
extern s32 *func_00107D60(s32 first, s32 last, s32 *predecessors);
extern void func_0011AECC(RuntimeUnit *unit);
extern void func_0012C788(u8 selector, RuntimeUnit *unit, s32 context, s32 mode);

static inline s32 current_goal(RuntimeUnit *unit)
{
    if (FIELD(unit, u8, 0x91) == 1 && FIELD(unit, s32, 0x84) != -1)
        return FIELD(unit, s32, 0x64);
    return *(s32 *)((u32)unit + (FIELD(unit, s32, 0x24) << 2) + 0x54);
}

/* Numeric views retain the observed 64-byte auxiliary allocation, its four
 * child buffers, and the existing bank/row strides without capacity claims. */
void func_0012D170(RuntimeUnit *unit, u8 selector, s32 *state, s32 context)
{
    u32 clearMask = ~0x40000U;
    u32 originalFlags = FIELD(unit, u32, 0x00);
    u32 clearedFlags = originalFlags & clearMask;
    MovementAux *aux;
    MovementAux *current;
    FIELD(unit, u32, 0x00) = clearedFlags;

    if (originalFlags & 0x1000) {
        FIELD(unit, u32, 0x00) = clearedFlags | 0x40000;
        func_00126D24(unit);
        return;
    }
    if (FIELD(unit, u8, 0x20) == 0) {
        u32 flags;
        FIELD(unit, u32, 0x00) = clearedFlags | 0x40000;
        func_00106CE0(&FIELD(unit, f32, 0x18), &FIELD(unit, f32, 0x1C),
                     1.0f / (f32)func_0012E8EC(unit));
        if (FIELD(unit, f32, 0x18) == FIELD(unit, f32, 0x1C)) {
            flags = FIELD(unit, u32, 0x00);
            flags &= clearMask;
            flags &= ~2U;
            FIELD(unit, u32, 0x00) = flags;
        }
        return;
    }
    if ((FIELD(unit, u8, 0xB8) & 0xF0) == 0x10 &&
        FIELD(unit, u8, 0xB9) != 0)
        return;

    current = FIELD(unit, MovementAux *, 0xA0);
    FIELD(unit, u32, 0x00) = clearedFlags | 0x40000;
    if (current == 0) {
        if (*state != 0)
            return;
        aux = FIELD(unit, MovementAux *, 0xA0) = resource_alloc(0x40);
        if (aux != 0) {
            FIELD(aux, s32, 0x00) = FIELD(unit, s32, 0x14);
            FIELD(aux, s32, 0x04) = current_goal(unit);
            FIELD(aux, s32, 0x10) = 0x3F;
            FIELD(aux, s32, 0x14) = 0x3F;
            FIELD(aux, s32, 0x08) = 0;
            FIELD(aux, s32, 0x0C) = 0;
            FIELD(aux, void *, 0x18) = D_801F0D98;
            FIELD(aux, void *, 0x1C) = resource_alloc(D_801F0DA8 * D_801F0DAC);
            FIELD(aux, void *, 0x20) = resource_alloc(D_801F0DA8 * (D_801F0DAC << 2));
            FIELD(aux, void *, 0x24) = resource_alloc(D_801F0DA8 * (D_801F0DAC << 2));
            {
                s32 height = D_801F0DAC;
                s32 width = D_801F0DA8;
                height *= 12;
                FIELD(aux, void *, 0x30) = resource_alloc(width * height);
            }
            FIELD(aux, s32, 0x3C) = 0;
            FIELD(aux, s32, 0x28) = 0;
            FIELD(aux, s32, 0x2C) = 0;
        }
        {
            u32 row = selector;
            u32 bank;
            row <<= 6;
            bank = D_800E7A90[0x33];
            D_801F0DB4 = (u16 *)((u32)D_801F36D8 + row);
            D_801F0DB0 = *(u8 **)(D_800E7A90 + bank * 4);
        }
        if (aux != 0) {
            if (FIELD(aux, void *, 0x1C) == 0 ||
                FIELD(aux, void *, 0x20) == 0 ||
                FIELD(aux, void *, 0x24) == 0)
                goto allocation_failed;
            if (FIELD(aux, void *, 0x30) != 0)
                goto allocation_ready;
        }
allocation_failed:
        if (aux != 0) {
            if (FIELD(aux, void *, 0x1C) != 0) {
                resource_free(FIELD(aux, void *, 0x1C));
                FIELD(aux, void *, 0x1C) = 0;
            }
            if (FIELD(aux, void *, 0x20) != 0) {
                resource_free(FIELD(aux, void *, 0x20));
                FIELD(aux, void *, 0x20) = 0;
            }
            if (FIELD(aux, void *, 0x24) != 0) {
                resource_free(FIELD(aux, void *, 0x24));
                FIELD(aux, void *, 0x24) = 0;
            }
            if (FIELD(aux, void *, 0x30) != 0) {
                resource_free(FIELD(aux, void *, 0x30));
                FIELD(aux, void *, 0x30) = 0;
            }
        }
        current = FIELD(unit, MovementAux *, 0xA0);
        if (current != 0) {
            resource_free(current);
            FIELD(unit, MovementAux *, 0xA0) = 0;
        }
        return;
allocation_ready:
        current = FIELD(unit, MovementAux *, 0xA0);
        goto dispatch;
    }
    if (FIELD(current, s32, 0x28) != 0) {
        if (*state != 0)
            return;
dispatch:
        func_00107E38(current);
        *state = 1;
        return;
    }
    if (FIELD(unit, void *, 0x68) != 0) {
        func_0012C788(D_800E7AB9, unit, context, 0);
        return;
    }

    aux = current;
    if (FIELD(unit, u8, 0x91) == 1 && FIELD(unit, s32, 0x84) != -1) {
        if (FIELD(FIELD(aux, u8 *, 0x1C), u8, FIELD(unit, s32, 0x64)) == 0) {
            if ((FIELD(unit, u8, 0xB8) & 0xF0) != 0x10) {
                FIELD(unit, u8, 0xB8) = 0x10;
                FIELD(unit, u8, 0xB9) = 2;
                func_001072B8(unit);
                FIELD(unit, s32, 0x84) = -1;
            }
        }
    } else if ((FIELD(unit, u32, 0x00) & 0x204) == 4) {
        /* This volatile view keeps the observed byte read before the word
         * read. It adds no access and asserts no original qualifier or
         * concurrent access policy. */
        u8 expectedCount = FIELD(unit, volatile u8, 0x20);
        s32 index = FIELD(unit, s32, 0x24);
        if (index == expectedCount &&
            FIELD(FIELD(aux, u8 *, 0x1C), u8,
                  *(s32 *)((u32)unit + (index << 2) + 0x54)) == 0) {
            if ((FIELD(unit, u8, 0xB8) & 0xF0) != 0x10) {
                s32 position;
                RuntimeUnit **slot;
                u8 *point;
                FIELD(unit, u8, 0xB8) = 0x10;
                FIELD(unit, u8, 0xB9) = 2;
                func_001072B8(unit);
                position = FIELD(unit, s32, 0x24) - 1;
                slot = &D_801F0CB0[FIELD(unit, s32, 0x80)];
                point = (u8 *)unit + position * 12;
                FIELD(point, f32, 0x28) = FIELD(*slot, f32, 0x08);
                FIELD(point, f32, 0x2C) = FIELD(*slot, f32, 0x0C);
                FIELD(point, f32, 0x30) = FIELD(*slot, f32, 0x10);
                FIELD(unit, s32, 0x58 + position * 4) = FIELD(*slot, s32, 0x14);
            }
        }
    }
    if ((FIELD(unit, u8, 0xB8) & 0xF0) == 0x10 &&
        FIELD(unit, u8, 0xB9) != 0)
        return;
    {
        s32 *list;
        if (FIELD(unit, u8, 0x91) == 1 && FIELD(unit, s32, 0x84) != -1)
            list = func_00107D60(FIELD(unit, s32, 0x14), FIELD(unit, s32, 0x64),
                                FIELD(aux, s32 *, 0x24));
        else
            list = func_00107D60(FIELD(unit, s32, 0x14),
                                *(s32 *)((u32)unit + (FIELD(unit, s32, 0x24) << 2) + 0x54),
                                FIELD(aux, s32 *, 0x24));
        if (list != 0) {
            FIELD(unit, s32 *, 0x68) = list;
            resource_free(FIELD(aux, void *, 0x1C));
            FIELD(aux, void *, 0x1C) = 0;
            resource_free(FIELD(aux, void *, 0x20));
            FIELD(aux, void *, 0x20) = 0;
            resource_free(FIELD(aux, void *, 0x24));
            FIELD(aux, void *, 0x24) = 0;
            resource_free(FIELD(aux, void *, 0x30));
            FIELD(aux, void *, 0x30) = 0;
            func_0011AECC(unit);
            FIELD(unit, s32, 0x6C) = 1;
        }
    }
    return;

}
