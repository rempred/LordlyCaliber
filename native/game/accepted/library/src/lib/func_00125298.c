typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;
typedef struct MovementAux MovementAux;
#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))
extern RuntimeUnit *D_801F0CB0[];
/* Two adjacent signed dimension words; no grid capacity is inferred. */
extern s32 D_801F0DA8[2];
extern f32 D_801F3A38;
extern char D_801EE710[];
extern void *resource_alloc(u32 bytes);
extern void resource_free(void *object);
extern void func_00023940(const char *format, ...);
extern f32 func_0002CB80(f32 first, f32 second);
/* Only the two passed arguments are used here; the return is ignored. */
extern void func_801D0880(u8 *buffer, s32 index);

s32 func_00125298(RuntimeUnit *unit)
{
    s32 *list;
    s32 position;
    RuntimeUnit *other;
    u8 *buffer = FIELD(FIELD(unit, MovementAux *, 0xA0), u8 *, 0x3C);

    if (buffer != 0) {
        resource_free(buffer);
        FIELD(FIELD(unit, MovementAux *, 0xA0), u8 *, 0x3C) = 0;
    }
    list = FIELD(unit, s32 *, 0xA8);
    position = 0;
    if (list == 0)
        return 1;
    for (;;) {
        s32 index = list[position];
        if (index == -1)
            return 1;
        if (index != FIELD(unit, s32, 0x80)) {
            other = D_801F0CB0[index];
            if ((FIELD(other, u32, 0x00) & 0x31) == 0x31) {
                if (func_0002CB80(FIELD(unit, f32, 0x08) - FIELD(other, f32, 0x08),
                           FIELD(unit, f32, 0x10) - FIELD(other, f32, 0x10)) <= D_801F3A38 * 3.0f) {
                    if (FIELD(FIELD(unit, MovementAux *, 0xA0), u8 *, 0x3C) == 0) {
                        s32 i;
                        FIELD(FIELD(unit, MovementAux *, 0xA0), u8 *, 0x3C) = resource_alloc(D_801F0DA8[0] * D_801F0DA8[1]);
                        if (FIELD(FIELD(unit, MovementAux *, 0xA0), u8 *, 0x3C) == 0) {
                            func_00023940(D_801EE710);
                            return 0;
                        }
                        /* Keep the global product at each guard. GCC forms the
                         * allocation and clear-loop address lifetimes separately. */
                        for (i = 0; i < D_801F0DA8[0] * D_801F0DA8[1]; ++i)
                            FIELD(FIELD(unit, MovementAux *, 0xA0), u8 *, 0x3C)[i] = 0;
                    }
                    func_801D0880(FIELD(FIELD(unit, MovementAux *, 0xA0), u8 *, 0x3C), FIELD(other, s32, 0x14));
                }
            }
        }
        ++position;
    }
}
