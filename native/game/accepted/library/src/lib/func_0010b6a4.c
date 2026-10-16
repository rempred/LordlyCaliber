#include "common/types.h"

typedef struct {
    u32 flags;
    u8 field_04;
    u8 field_05[0x6F];
    s32 field_74;
    u8 field_78[0x19];
    u8 field_91;
    u8 field_92;
    u8 field_93[5];
    u8 field_98;
} ScenarioB040Record;

extern u8 b6a4_icache_start[], b6a4_icache_end[];
extern u8 b6a4_dcache_start[], b6a4_dcache_end[];
extern u8 b6a4_dma_destination[], b6a4_bss_start[], b6a4_bss_end[];
extern u8 D_00188B60[], D_0018F100[];
extern ScenarioB040Record *D_801F0CB0[];
extern u8 D_801969BA[][11];
extern s32 D_801F3658, D_801F0DE0;
extern u8 D_801F365D;

void func_800900C0(void *, s32);
void func_80090010(void *, s32);
void func_8009DA50(void *, void *, s32);
void func_80093380(void *, s32);
s32 func_801DA228(ScenarioB040Record *);
void *func_80070F30(u32);
void func_800712C4(void *);
void func_80218700(s32 *);

void func_0010B6A4(void)
{
    ScenarioB040Record *record;
    ScenarioB040Record *other;
    s32 value, count, i, selected;
    u32 flags;
    s32 *list;

    func_800900C0(b6a4_icache_start, b6a4_icache_end - b6a4_icache_start);
    func_80090010(b6a4_dcache_start, b6a4_dcache_end - b6a4_dcache_start);
    func_8009DA50(D_00188B60, b6a4_dma_destination, D_0018F100 - D_00188B60);
    if (b6a4_bss_start != b6a4_bss_end) {
        func_80093380(b6a4_bss_start, b6a4_bss_end - b6a4_bss_start);
    }
    selected = D_801F3658;
    D_801F0DE0 = 0x27;
    record = *(ScenarioB040Record **)((u8 *)D_801F0CB0 + selected * sizeof(*D_801F0CB0));
    if (!(*(u32 *)record & 0x40)) {
        value = record->field_74;
    } else {
        value = D_801F0CB0[D_801969BA[func_801DA228(record)][0]]->field_74;
    }
    for (i = 0, count = 0; i < 30; i++) {
        other = D_801F0CB0[i];
        if (record->field_04 != i) {
            flags = *(u32 *)other;
            if (flags & 1) {
                if ((flags & 0x50) == 0x10) {
                    if (!(flags & 2)) {
                        count += other->field_74 == value;
                    }
                }
            }
        }
    }
    list = func_80070F30((count + 1) * sizeof(*list));
    for (i = 0, count = 0; i < 30; i++) {
        other = D_801F0CB0[i];
        if (record->field_04 != i) {
            flags = *(u32 *)other;
            if (flags & 1) {
                if ((flags & 0x50) == 0x10) {
                    if (!(flags & 2)) {
                        if (other->field_74 == value) {
                            list[count++] = other->field_04;
                        }
                    }
                }
            }
        }
    }
    list[count] = -1;
    func_80218700(list);
    func_800712C4(list);
    D_801F365D |= 1;
}
