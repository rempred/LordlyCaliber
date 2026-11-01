typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef s32 M2C_UNK;
typedef s8 M2C_UNK8;
typedef s16 M2C_UNK16;
typedef s32 M2C_UNK32;
typedef s64 M2C_UNK64;
extern u8 D_80193BC0[];
s32 func_000bc984(void) {
    register s32 var_a0_70 asm("$4");
    register s32 var_a2_64 asm("$6");
    register s32 var_t4_7 asm("$12");
    register s32 var_t5_8 asm("$13");
    register s32 var_t6_11 asm("$14");
    register s32 var_t7_10 asm("$15");
    register s32 sentinel asm("$24");
    register s32 var_v0_56 asm("$2");
    register s32 var_v1_32 asm("$3");
    register u8 *temp_a3_24 asm("$7");
    register s32 temp_a0_16 asm("$4");
    register s32 temp_a1_21 asm("$5");
    register s32 temp_t2_22 asm("$10");
    register s32 var_t3_30 asm("$11");
    u8 var_v0_44;
    register u8 *temp_a1_66 asm("$5");
    register u32 temp_t0_62 asm("$8");
    register u8 *temp_t1_61 asm("$9");
    register void *temp_v0_15 asm("$2");
    register void *var_v0_71 asm("$2");

    var_t4_7 = 0;
    var_t5_8 = 0;
    sentinel = 0xFF;
    var_t7_10 = 0x1872;
    var_t6_11 = 0;
loop_1:
    temp_v0_15 = D_80193BC0 + var_t6_11;
    asm volatile(
        "# Hybrid boundary: all class walking, record scanning, range logic, flag writes, and the return value remain C.\n"
        "# Fixed-register annotations and documented empty constraints emit no bytes; only this lbu and one later andi emit code.\n"
        "# Hybrid scope: load the primary class byte at class-record offset 0x11 directly into $a0.\n"
        "# KMC otherwise loads through $a2 and emits an extra move, changing both the schedule and later index expression.\n"
        "lbu %0, 0x11(%1) # load and zero-extend the primary class byte into $a0\n"
        : "=r" (temp_a0_16) : "r" (temp_v0_15));
    var_v1_32 = *(u32 *)0x80196AF8;
    asm volatile(
        "# Hybrid scope: this zero-byte constraint keeps the global record base in $v1 in retail load order.\n"
        "# It emits no instruction and only prevents KMC from folding the load into the later $a3 address.\n"
        : : "r" (var_v1_32));
    temp_a1_21 = (*(u8 *)((s8 *)((temp_a0_16 + 0x801F0000)) + (-0xC48)));
    asm volatile(
        "# Hybrid scope: this zero-byte constraint commits the alternate class byte to $a1 before loading $t2.\n"
        "# The constraint emits no instruction and preserves the retail order of independent byte loads.\n"
        : : "r" (temp_a1_21));
    temp_t2_22 = (*(u8 *)((s8 *)(temp_v0_15) + (0x12)));
    temp_a3_24 = (u8 *)var_v1_32 + var_t7_10;
    if (temp_a1_21 != 0) {
        goto block_3;
    }
    var_t3_30 = (*(u8 *)((s8 *)(((temp_a0_16 * 0x48) + 0x80180000)) + (0x7C59)));
    var_v1_32 = var_t4_7 == 0;
    goto block_4;
block_3:
    var_t3_30 = temp_a1_21;
    var_v1_32 = var_t4_7 == 0;
block_4:
    var_v0_56 = var_t4_7 < 0x64;
    var_v0_56 ^= 1;
    var_v1_32 |= var_v0_56;
    asm volatile(
        "# Hybrid scope: these zero-byte constraints retain retail's explicit $v0 boolean inversion and $v1 OR.\n"
        "# They emit no instructions; without them KMC replaces the combined predicate with two short-circuit branches.\n"
        : : "r" (var_v0_56), "r" (var_v1_32));
    asm volatile(
        "# Hybrid scope: copy the secondary class byte from $t2 to $a0 with retail's explicit 0xFF mask.\n"
        "# KMC proves the preceding byte load already masked and substitutes a move, deleting this retail instruction.\n"
        "andi %0, %1, 0x00FF # copy and zero-extend the secondary class byte from $t2 into $a0\n"
        : "=r" (temp_a0_16) : "r" (temp_t2_22));
    if (var_v1_32 == 0) {
        goto block_6;
    }
    var_v0_44 = *temp_a3_24;
    asm volatile(
        "# Hybrid scope: this zero-byte marker consumes the initial flag byte at its retail site.\n"
        "# It emits no instruction; the value dependency prevents KMC from tail-merging this load with later flag updates.\n"
        : : "r" (var_v0_44));
    var_v0_44 |= 0x80;
    goto block_33;
block_6:
    if (temp_a0_16 == 0) {
        goto block_34;
    }
    var_v1_32 = *temp_a3_24;
    if (!(var_v1_32 & 0x10)) {
        goto block_10;
    }
    var_v0_56 = var_v1_32 | 0x80;
    *temp_a3_24 = var_v0_56;
    goto block_34;
block_9:
    var_v0_56 = var_v1_32;
    goto block_18;
block_10:
    var_v0_56 = var_v1_32 & 8;
    if (var_v0_56 == 0) {
        goto block_27;
    }
    var_v1_32 = 0;
    temp_t1_61 = *(u8 **)0x80196AF8;
    temp_t0_62 = var_t4_7 & 0xFF;
    var_a2_64 = 0x117C;
loop_12:
    temp_a1_66 = temp_t1_61 + var_a2_64;
    if ((*(u8 *)((s8 *)(temp_a1_66) + (3))) == sentinel) {
        goto block_16;
    }
    var_a0_70 = 0;
loop_14:
    var_v0_71 = temp_a1_66 + var_a0_70;
    var_a0_70 += 1;
    if ((*(u8 *)((s8 *)(var_v0_71) + (4))) == temp_t0_62) {
        goto block_9;
    }
    if (var_a0_70 < 9) {
        goto loop_14;
    }
block_16:
    var_v1_32 += 1;
    var_a2_64 += 0x36;
    if (var_v1_32 < 0x1E) {
        goto loop_12;
    }
    var_v0_56 = 0xFF;
block_18:
    var_v1_32 = var_v0_56 & 0xFF;
    var_v0_56 = (var_v1_32 * 0x36) + 0x117C;
    var_v1_32 = *(u32 *)0x80196AF8;
    var_v1_32 += var_v0_56;
    var_v0_56 = *(u8 *)((u8 *)var_v1_32 + 3);
    var_v1_32 += var_v0_56;
    var_v0_56 = *(u8 *)((u8 *)var_v1_32 + 4);
    if (var_v0_56 != var_t4_7) {
        goto block_22;
    }
block_20:
    var_v0_44 = *temp_a3_24;
    asm volatile(
        "# Hybrid scope: this zero-byte marker consumes the scan-hit flag byte at its retail site.\n"
        "# It emits no instruction; the value dependency prevents KMC from merging this block with later flag updates.\n"
        : : "r" (var_v0_44));
    var_v0_44 |= 0x80;
    goto block_33;
block_22:
    var_v0_56 = temp_t2_22 - 0x24;
    asm volatile(
        "# Hybrid scope: this zero-byte constraint keeps the scanned-path class-range subtraction in $v0.\n"
        "# It emits no instruction and prevents destructive folding into the still-live secondary-class register $t2.\n"
        : : "r" (var_v0_56));
    var_v0_56 = (u32)var_v0_56 < 3U;
    if (var_v0_56 != 0) {
        goto block_26;
    }
    var_v0_56 = (u32)temp_t2_22 < 0x2AU;
    if (var_v0_56 != 0) {
        goto block_32;
    }
    var_v0_56 = var_t3_30 & 0xFF;
    if (var_v0_56 == 0) {
        goto block_26;
    }
    var_v1_32 = var_v0_56 != 0;
    var_v0_56 = (u32)var_v0_56 < 0x2AU;
    var_v0_56 ^= 1;
    var_v1_32 &= var_v0_56;
    asm volatile(
        "# Hybrid scope: these zero-byte constraints retain the scanned path's deliberately redundant nonzero/range predicate.\n"
        "# They emit no instructions and prevent KMC from replacing the source boolean chain with a shorter comparison.\n"
        : : "r" (var_v0_56), "r" (var_v1_32));
    if (var_v1_32 == 0) {
        goto block_32;
    }
block_26:
    var_v0_44 = *temp_a3_24;
    asm volatile(
        "# Hybrid scope: this zero-byte marker consumes the scanned-path flag byte at its retail site.\n"
        "# It emits no instruction; the value dependency prevents tail merging with the other three flag-set paths.\n"
        : : "r" (var_v0_44));
    var_v0_44 |= 0x80;
    goto block_33;
block_27:
    var_v0_56 = temp_t2_22 - 0x24;
    asm volatile(
        "# Hybrid scope: this zero-byte constraint keeps the no-scan class-range subtraction in $v0.\n"
        "# It emits no instruction and prevents KMC from destructively rewriting the live secondary-class value in $t2.\n"
        : : "r" (var_v0_56));
    var_v0_56 = (u32)var_v0_56 < 3U;
    if (var_v0_56 != 0) {
        goto block_31;
    }
    var_v0_56 = (u32)temp_a0_16 < 0x2AU;
    if (var_v0_56 != 0) {
        goto block_32;
    }
    var_v0_56 = var_t3_30 & 0xFF;
    if (var_v0_56 == 0) {
        goto block_31;
    }
    var_v1_32 = var_v0_56 != 0;
    var_v0_56 = (u32)var_v0_56 < 0x2AU;
    var_v0_56 ^= 1;
    var_v1_32 &= var_v0_56;
    asm volatile(
        "# Hybrid scope: these zero-byte constraints retain the no-scan path's deliberately redundant nonzero/range predicate.\n"
        "# They emit no instructions and prevent KMC from replacing the source boolean chain with a shorter comparison.\n"
        : : "r" (var_v0_56), "r" (var_v1_32));
    if (var_v1_32 == 0) {
        goto block_32;
    }
block_31:
    var_v0_44 = *temp_a3_24;
    asm volatile(
        "# Hybrid scope: this zero-byte marker consumes the no-scan flag byte at its retail site.\n"
        "# It emits no instruction; the value dependency prevents tail merging with the earlier flag-set paths.\n"
        : : "r" (var_v0_44));
    var_v0_44 |= 0x80;
    goto block_33;
block_32:
    var_t5_8 += 1;
    var_v0_44 = *temp_a3_24 & 0x7F;
block_33:
    *temp_a3_24 = var_v0_44;
block_34:
    var_t7_10 += 2;
    var_t4_7 += 1;
    var_t6_11 += 0x38;
    if (var_t4_7 < 0xDC) {
        goto loop_1;
    }
    return var_t5_8 & 0xFFFF;
}
