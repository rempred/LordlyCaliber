typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;

typedef struct { u32 flags; u8 source_index; } RuntimeUnit;
typedef struct { u8 bytes[11]; } DeploymentRow11;

extern DeploymentRow11 D_801969B8[];
extern RuntimeUnit *D_801F0CB0[];
extern s32 func_0012E968(RuntimeUnit *unit);

s32 func_0012DB2C(RuntimeUnit *unit)
{
    DeploymentRow11 *row = &D_801969B8[func_0012E968(unit)];
    s32 slot;

    for (slot = 0; slot < 5; slot++) {
        u32 member = row->bytes[slot + 2];
        if (member != 0xFF) {
            if (D_801F0CB0[member]->flags & 0x00020000) {
                return 1;
            }
        }
    }
    return 0;
}
