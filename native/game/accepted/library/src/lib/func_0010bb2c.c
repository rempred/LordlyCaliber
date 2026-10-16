#include "common/types.h"

typedef struct {
    u32 flags;
    u8 field_04[0x70];
    s32 field_74;
    u8 field_78[0x19];
    u8 field_91;
    u8 field_92;
    u8 field_93[5];
    u8 field_98;
} ScenarioB040Record;

extern u8 bb2c_icache_start[], bb2c_icache_end[];
extern u8 bb2c_dcache_start[], bb2c_dcache_end[];
extern u8 bb2c_dma_destination[], bb2c_bss_start[], bb2c_bss_end[];
extern u8 D_001977E0[], D_0019C760[];
extern ScenarioB040Record *D_801F0CB0[];
extern u8 D_801969BA[][11];
extern s32 D_801F3658, D_801F0DE0;
extern u8 D_801F365D;

void func_800900C0(void *, s32);
void func_80090010(void *, s32);
void func_8009DA50(void *, void *, s32);
void func_80093380(void *, s32);
s32 func_801DA228(ScenarioB040Record *);
void func_802195E0(s32);

void func_0010BB2C(void)
{
    ScenarioB040Record *record;
    s32 value, selected;

    func_800900C0(bb2c_icache_start, bb2c_icache_end - bb2c_icache_start);
    func_80090010(bb2c_dcache_start, bb2c_dcache_end - bb2c_dcache_start);
    func_8009DA50(D_001977E0, bb2c_dma_destination, D_0019C760 - D_001977E0);
    if (bb2c_bss_start != bb2c_bss_end) {
        func_80093380(bb2c_bss_start, bb2c_bss_end - bb2c_bss_start);
    }
    selected = D_801F3658;
    D_801F0DE0 = 0x24;
    record = *(ScenarioB040Record **)((u8 *)D_801F0CB0 + selected * sizeof(*D_801F0CB0));
    if (!(*(u32 *)record & 0x40)) {
        value = record->field_74;
    } else {
        value = D_801F0CB0[D_801969BA[func_801DA228(record)][0]]->field_74;
    }
    func_802195E0(value);
    D_801F365D |= 1;
}
