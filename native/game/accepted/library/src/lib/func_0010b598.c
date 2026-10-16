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

extern u8 b598_icache_start[], b598_icache_end[];
extern u8 b598_dcache_start[], b598_dcache_end[];
extern u8 b598_dma_destination[], b598_bss_start[], b598_bss_end[];
extern u8 D_0019C760[], D_001A2C20[];
extern ScenarioB040Record *D_801F0CB0[];
extern u8 D_801969BA[][11];
extern s32 D_801F3658, D_801F0DE0;

void func_800900C0(void *, s32);
void func_80090010(void *, s32);
void func_8009DA50(void *, void *, s32);
void func_80093380(void *, s32);
s32 func_801DA228(ScenarioB040Record *);
void func_8021B1B0(s32);

void func_0010B598(void)
{
    ScenarioB040Record *record;
    s32 value;

    func_800900C0(b598_icache_start, b598_icache_end - b598_icache_start);
    func_80090010(b598_dcache_start, b598_dcache_end - b598_dcache_start);
    func_8009DA50(D_0019C760, b598_dma_destination, D_001A2C20 - D_0019C760);
    if (b598_bss_start != b598_bss_end) {
        func_80093380(b598_bss_start, b598_bss_end - b598_bss_start);
    }
    record = D_801F0CB0[D_801F3658];
    D_801F0DE0 = 0x28;
    if (!(*(u32 *)record & 0x40)) {
        value = record->field_74;
    } else {
        value = D_801F0CB0[D_801969BA[func_801DA228(record)][0]]->field_74;
    }
    func_8021B1B0(value);
}
