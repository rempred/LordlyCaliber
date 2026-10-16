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

typedef struct {
    s8 field_0;
    s8 field_1;
    u8 field_2;
    u8 field_3;
    s8 field_4;
    s8 field_5;
} __attribute__((packed)) ScenarioB040Request;

extern u8 b040_icache_start[], b040_icache_end[];
extern u8 b040_dcache_start[], b040_dcache_end[];
extern u8 b040_dma_destination[], b040_bss_start[], b040_bss_end[];
extern u8 D_00165FC0[], D_00171EA0[];
extern ScenarioB040Record *D_801F0CB0[];
extern s32 D_801F3658, D_801F0DE0;
extern u8 D_801F365D, D_80210219;

void func_800900C0(void *, s32);
void func_80090010(void *, s32);
void func_8009DA50(void *, void *, s32);
void func_80093380(void *, s32);
void func_8021A270(ScenarioB040Request);

void func_0010B040(void)
{
    ScenarioB040Request request;
    ScenarioB040Record *record;

    func_800900C0(b040_icache_start, b040_icache_end - b040_icache_start);
    func_80090010(b040_dcache_start, b040_dcache_end - b040_dcache_start);
    func_8009DA50(D_00165FC0, b040_dma_destination, D_00171EA0 - D_00165FC0);
    if (b040_bss_start != b040_bss_end) {
        func_80093380(b040_bss_start, b040_bss_end - b040_bss_start);
    }
    record = D_801F0CB0[D_801F3658];
    D_801F0DE0 = 0x2B;
    /* Keep separate nibble writes: the pinned compiler folds the byte store
     * but retains two unused homes, reproducing the retail frame. */
    D_80210219 = (D_80210219 & 0xF0) | 2;
    D_80210219 = (D_80210219 & 0x0F) | 0x20;
    D_801F365D |= 1;
    /* Retail writes only bytes 2..5 of this six-byte argument. */
    if (record->flags & 0x2000) {
        request.field_2 = record->field_98;
    } else {
        request.field_2 = record->field_91;
    }
    request.field_3 = record->field_92;
    request.field_4 = record->field_74 != -1;
    if (record->flags & 0x2002) {
        request.field_5 = 1;
    } else {
        request.field_5 = 0;
    }
    func_8021A270(request);
}
