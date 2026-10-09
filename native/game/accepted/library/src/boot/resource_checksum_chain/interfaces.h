#ifndef CHECKSUM_CHAIN_PRIVATE_INTERFACES_H
#define CHECKSUM_CHAIN_PRIVATE_INTERFACES_H
#include "common/types.h"

/* Numeric prefix view, independently supported by the observed accesses. */
typedef struct ProbeRecordPrefix {
    u16 word_00;
    u16 word_02;
    u8 bytes_04[8];
} ProbeRecordPrefix;

extern void *resource_alloc(u32 bytes);
extern void resource_free(void *record);
extern void func_0000539C(s32 id);
extern void func_0001A4F0(s32 offset, void *destination, u32 bytes, s32 mode);
extern s32 func_00023350(const void *first, const void *second, u32 bytes);
/* The accepted copy sources discard the callee's observed A1 return value. */
extern void func_00023460(const void *source, void *destination, u32 bytes);
extern u8 *D_800A83B8;
extern u8 D_800A83BC;
extern u8 D_800A8240[];

s32 func_00005760(s32 id);
s32 func_0000581C(s32 id, void *record);
s32 func_00005978(void *record);
s32 func_00005A88(void *record);
void func_00005D9C(s32 id, void *record);
u32 func_00005E84(const u8 *bytes, u32 count, u32 seed);
u32 func_00005EC4(const u8 *bytes, u32 count);
u16 func_00005F00(const u8 *bytes, u16 count, u32 seed);
u16 func_00005F60(const u8 *bytes, u16 count);
#endif
