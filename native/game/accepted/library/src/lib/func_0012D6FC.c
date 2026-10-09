typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef struct RuntimeUnit RuntimeUnit;

/* A numeric view of the complete 64-byte allocation. No field meaning or
 * grid/index capacity is inferred beyond the observed offsets and strides. */
typedef struct MovementAux MovementAux;

#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))
extern u8 D_800E7AB9;
/* The observed bank has its selector byte at +0x33; pointer entries use
 * four-byte offsets from this base. This view assigns no entry capacity. */
extern u8 D_800E7A90[];
extern u8 D_801F0D98[];
extern s32 D_801F0DA8, D_801F0DAC;
extern u8 *D_801F0DB0;
extern u16 *D_801F0DB4;
extern u16 D_801F36D8[];
extern void *resource_alloc(u32 bytes);
extern void resource_free(void *object);
extern s32 *func_00107D60(s32 first, s32 second, void *third);
extern s32 func_00107E38(MovementAux *object);
extern void func_0011AECC(RuntimeUnit *unit);
extern void func_0012C788(u8 first, RuntimeUnit *unit, s32 third, s32 fourth);

void func_0012D6FC(RuntimeUnit *unit, u8 selector, s32 *state)
{
    /* This pointer is shared by allocation and completed-route cleanup,
     * matching the original saved-pointer lifetime. */
    MovementAux *aux;
    MovementAux *current = FIELD(unit, MovementAux *, 0xA0);

    if (current == 0) {
        if (*state == 0) {
            aux = FIELD(unit, MovementAux *, 0xA0) = resource_alloc(0x40);
            if (aux != 0) {
                s32 value;
                FIELD(aux, s32, 0x00) = FIELD(unit, s32, 0x14);
                value = *(s32 *)((u32)unit + (FIELD(unit, s32, 0x24) << 2) + 0x54);
                FIELD(aux, s32, 0x10) = 0x3F;
                FIELD(aux, s32, 0x14) = 0x3F;
                FIELD(aux, s32, 0x08) = 0;
                FIELD(aux, s32, 0x0C) = 0;
                FIELD(aux, void *, 0x18) = D_801F0D98;
                FIELD(aux, s32, 0x04) = value;
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
                u32 bank_index;
                row <<= 6;
                bank_index = D_800E7A90[0x33];
                D_801F0DB4 = (u16 *)((u32)D_801F36D8 + row);
                /* Keep the four-byte scale as multiplication: KMC CSE then
                 * shares the selector/address base. A shift splits it. */
                D_801F0DB0 = *(u8 **)(D_800E7A90 + (bank_index * 4));
            }

            if (aux != 0) {
                if (FIELD(aux, void *, 0x1C) == 0 || FIELD(aux, void *, 0x20) == 0 ||
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
            if (FIELD(unit, MovementAux *, 0xA0) != 0) {
                resource_free(FIELD(unit, MovementAux *, 0xA0));
                FIELD(unit, MovementAux *, 0xA0) = 0;
            }
            goto finished;
allocation_ready:
            current = FIELD(unit, MovementAux *, 0xA0);
            goto dispatch;
        }
    } else if (FIELD(current, s32, 0x28) != 0) {
        if (*state == 0) {
dispatch:
            func_00107E38(current);
            *state = 1;
        }
    } else if (FIELD(unit, void *, 0x68) != 0) {
        func_0012C788(D_800E7AB9, unit, 5, 1);
    } else {
        s32 *result;
        aux = current;
        result = func_00107D60(FIELD(unit, s32, 0x14),
                                  *(s32 *)((u32)unit + (FIELD(unit, s32, 0x24) << 2) + 0x54),
                                  FIELD(aux, void *, 0x24));
        if (result != 0) {
            FIELD(unit, s32 *, 0x68) = result;
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
finished:
    return;
}
