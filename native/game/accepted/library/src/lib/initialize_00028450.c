typedef unsigned int u32;
typedef unsigned char u8;
typedef unsigned long long u64;
typedef struct { u32 words[4]; } Vector16;
typedef struct {
    u8 unknown_00[4];
    u8 field_04, field_05, field_06, field_07, field_08;
} SpeedParamPrefix;

extern u32 D_800E82C0;
extern u64 D_800ABAC0;
extern u32 D_800ABAC8;
extern u8 D_8000031C[];
extern Vector16 osException_vector;
extern SpeedParamPrefix __Dom1SpeedParam, __Dom2SpeedParam;
extern u32 __osGetSR(void);
extern void __osSetSR(u32);
extern void __osSetFpcCsr(u32);
extern void __osSetWatchLo(u32);
extern int func_0002AC30(u32, u32 *);
extern int func_0002AC80(u32, u32);
extern void os_writeback_dcache(void *, int);
extern void os_inval_icache(void *, int);
extern void func_8009AB00(void);
extern void osMapTLBRdb(void);
extern void func_80093380(void *, int);
extern u32 osGetCause(void);
void read_pi_registers(void);

void func_00028450(void)
{
    u32 value;
    D_800E82C0 = 1;
    __osSetSR(__osGetSR() | 0x20000000);
    __osSetFpcCsr(0x01000800);
    __osSetWatchLo(0x04900000);
    while (func_0002AC30(0x1FC007FC, &value) != 0) { }
    while (func_0002AC80(0x1FC007FC, value | 8) != 0) { }
    *(Vector16 *)0x80000000 = osException_vector;
    *(Vector16 *)0x80000080 = osException_vector;
    *(Vector16 *)0x80000100 = osException_vector;
    *(Vector16 *)0x80000180 = osException_vector;
    os_writeback_dcache((void *)0x80000000, 0x190);
    os_inval_icache((void *)0x80000000, 0x190);
    read_pi_registers();
    func_8009AB00();
    osMapTLBRdb();
    D_800ABAC0 = D_800ABAC0 * 3 / 4;
    if (*(u32 *)0x8000030C == 0)
        func_80093380(D_8000031C, 0x40);
    if (*(u32 *)0x80000300 == 0)
        D_800ABAC8 = 0x02F5B2D2;
    else if (*(u32 *)0x80000300 == 2)
        D_800ABAC8 = 0x02E6025C;
    else
        D_800ABAC8 = 0x02E6D354;
    if (osGetCause() & 0x1000)
        for (;;) { }
    *(volatile u32 *)0xA4500008 = 1;
    *(volatile u32 *)0xA4500010 = 0x3FFF;
    *(volatile u32 *)0xA4500014 = 0xF;
}

void read_pi_registers(void)
{
    u32 a = *(volatile u32 *)0xA4600014;
    u32 b = *(volatile u32 *)0xA4600018;
    u32 c = *(volatile u32 *)0xA460001C;
    u32 d = *(volatile u32 *)0xA4600020;
    u32 e = *(volatile u32 *)0xA4600024;
    u32 f = *(volatile u32 *)0xA4600028;
    u32 g = *(volatile u32 *)0xA460002C;
    u32 h = *(volatile u32 *)0xA4600030;
    __Dom1SpeedParam.field_04 = 7;
    __Dom2SpeedParam.field_04 = 7;
    __Dom1SpeedParam.field_05 = a;
    __Dom1SpeedParam.field_08 = b;
    __Dom1SpeedParam.field_06 = c;
    __Dom1SpeedParam.field_07 = d;
    __Dom2SpeedParam.field_05 = e;
    __Dom2SpeedParam.field_08 = f;
    __Dom2SpeedParam.field_06 = g;
    __Dom2SpeedParam.field_07 = h;
}
