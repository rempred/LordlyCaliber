typedef unsigned char u8;
typedef signed char s8;
typedef signed int s32;
typedef unsigned int u32;

typedef struct {
    u8 bytes[0x19];
} SourceRecord;

typedef struct {
    u8 unk_00[2];
    u8 id;
    u8 unk_03[4];
    s8 value;
} WorkingRecord;

extern u8 *D_801CE8C0;
extern u8 D_801976DC;
extern u8 D_801976E8;
extern SourceRecord g_func_001957D0_source_records[];

void *func_0020C478(s32 index);
s32 func_0020C014(void *actor);
s32 func_0020BFF8(void *actor);
s32 func_0020C2C0(void *actor);

void func_00201108(void)
{
    void *actor;
    s32 first_limit;
    void *entry;
    volatile WorkingRecord *record;
    WorkingRecord *first_record_end;
    WorkingRecord *second_record_end;
    s32 actor_index;

    *(SourceRecord *)(D_801CE8C0 + 0x82F) =
        g_func_001957D0_source_records[D_801976DC];
    *(SourceRecord *)(D_801CE8C0 + 0x848) =
        g_func_001957D0_source_records[D_801976E8];

    record = D_801CE8C0 + 0x82F;
    first_record_end = D_801CE8C0 + 0x834;
    first_limit = 8;
    do {
        actor_index = 0;
        if (record->id != 0) {
            entry = (void *)record;
            do {
                actor = func_0020C478(actor_index);
                if (actor != 0 && func_0020C014(actor) != 0 &&
                    *(u8 *)((s8 *)actor + 0xF6) == *(u8 *)((s8 *)entry + 2)) {
                    if (func_0020C2C0(actor) != 0) {
                        *(u8 *)((s8 *)entry + 2) = 0;
                    } else {
                        *(s8 *)((s8 *)entry + 7) =
                            (s8)((u32)((first_limit - *(s32 *)((s8 *)actor + 0x58)) * 3) -
                                 ((u32)*(s32 *)((s8 *)actor + 0x54) - 2));
                        break;
                    }
                }
                actor_index++;
            } while (actor_index < 20);
            if (actor_index >= 20) {
                record->id = 0;
            }
        }
        record = (volatile WorkingRecord *)((s8 *)record + 1);
    } while ((u32)record < (u32)first_record_end);

    record = D_801CE8C0 + 0x848;
    second_record_end = D_801CE8C0 + 0x84D;
    do {
        actor_index = 0;
        if (record->id != 0) {
            entry = (void *)record;
            do {
                actor = func_0020C478(actor_index);
                if (actor != 0 && func_0020BFF8(actor) != 0 &&
                    *(u8 *)((s8 *)actor + 0xF6) == *(u8 *)((s8 *)entry + 2)) {
                    if (func_0020C2C0(actor) != 0) {
                        *(u8 *)((s8 *)entry + 2) = 0;
                    } else {
                        *(s8 *)((s8 *)entry + 7) =
                            (s8)(*(s32 *)((s8 *)actor + 0x58) * 3 +
                                 *(s32 *)((s8 *)actor + 0x54));
                        break;
                    }
                }
                actor_index++;
            } while (actor_index < 20);
            if (actor_index >= 20) {
                record->id = 0;
            }
        }
        record = (volatile WorkingRecord *)((s8 *)record + 1);
    } while ((u32)record < (u32)second_record_end);
}
