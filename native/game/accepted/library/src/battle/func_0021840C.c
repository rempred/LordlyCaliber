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
typedef union {
    volatile u32 read;
    u32 write;
} BattleFlags;
typedef struct {
    u8 pad_0000[0x604C];
    BattleFlags flags_604C;
} BattleState;
struct _m2c_stack_func_0021840C {
               char pad0[0x10];
               s32 sp10;                                          
               s32 sp14;                                          
               M2C_UNK sp18;                                      
               char pad18[4];
               M2C_UNK sp1C;                                      
               char pad1C[0x34];
};                                                                   

M2C_UNK func_001F0E64(void *, M2C_UNK);                         
M2C_UNK func_0020BD5C(M2C_UNK);                                 
s32 func_0020BF7C();                                            
s32 func_0020BF8C();                                            
s32 func_0020BFE4();                                            
s32 func_0020C014(void **);                                     
s32 func_0020C2C0(void **);                                     
s32 func_0020C32C(void **);                                     
void **func_0020C478(s32);                                      
M2C_UNK func_0021824C(s32 *, s32 *, M2C_UNK *, M2C_UNK *);             
M2C_UNK func_0021EAF0(void **);                                 
u16 func_002224F4();                                            

extern void *D_801CE8BC;
extern u8 D_801CEAAB;
extern s8 D_801CEAAC;
extern s8 D_801CEAAD;
extern s8 D_801CEAAE;
extern u16 D_801D0758;
extern volatile u16 D_801D075C;

void func_0021840C(void) {
    s32 sp10;
    s32 sp14;
    M2C_UNK sp18;
    M2C_UNK sp1C;
    M2C_UNK var_a0_252;
    s16 temp_v0_98;
    s32 var_v0_73;
    s32 temp_v1_151;
    s32 var_a0_102;
    s32 var_s1_224;
    s32 var_s1_67;
    s32 var_s3_49;
    s32 var_s4_19;
    s32 state_20;
    s32 state_22;
    s32 terminal_state;
    u32 flags_604C;
    s32 var_v0_117;
    s32 var_a1_179;
    u16 temp_v0_250;
    s8 *temp_s1_93;
    void **temp_v0_226;
    void **temp_v0_54;
    s8 *var_s0_68;
    s8 *var_s0_92;
    void *temp_a0_70;
    BattleState *temp_a2_150;
    void *temp_v0_95;
    void *temp_v1_23;

    if (func_0020BFE4() != 0) {
        var_s4_19 = 0x3C;
    } else {
        var_s4_19 = 0x1E;
    }
    temp_v1_23 = D_801CE8BC;
    if ((*(s32 *)((s8 *)(temp_v1_23) + (0x6048))) == 0) {
        D_801CEAAE = 0;
        D_801CEAAD = 0;
        D_801CEAAC = 0;
        D_801CEAAB = 0;
    }
    (*(s32 *)((s8 *)(temp_v1_23) + (0x6048))) = (s32) ((*(s32 *)((s8 *)(temp_v1_23) + (0x6048))) + 1);
    func_0021824C(&sp10, &sp14, &sp18, &sp1C);
    if ((*(s32 *)((s8 *)D_801CE8BC + (0x6048))) < 0x4C) {
        var_s3_49 = 0;
        state_20 = 0x20;
        state_22 = 0x22;
        terminal_state = 3;
        do {
            temp_v0_54 = func_0020C478(var_s3_49);
            if ((func_0020C2C0(temp_v0_54) == 0) && (func_0020C32C(temp_v0_54) == 0)) {
                func_0021EAF0(temp_v0_54);
                var_s1_67 = 0;
                var_s0_68 = (s8 *) temp_v0_54;
                do {
                    temp_a0_70 = *(void **) var_s0_68;
                    if (temp_a0_70 != 0) {
                        var_v0_73 = (*(s16 *)((s8 *)(temp_a0_70) + (0x48)));
                        if (var_v0_73 == state_20) {
                            temp_a0_70 += 0x44;
                            func_001F0E64(temp_a0_70, 0x21);
                            temp_a0_70 = *(void **) var_s0_68;
                            var_v0_73 = (*(s16 *)((s8 *) temp_a0_70 + 0x48));
                        }
                        if (var_v0_73 == state_22) {
                            temp_a0_70 += 0x44;
                            func_001F0E64(temp_a0_70, 0x23);
                        }
                    }
                    var_s1_67 += 1;
                    var_s0_68 += 4;
                } while (var_s1_67 < 3);
                var_s0_92 = (s8 *) temp_v0_54;
                temp_s1_93 = var_s0_92 + 0xC;
                do {
                    temp_v0_95 = *(void **) var_s0_92;
                    if (temp_v0_95 != 0) {
                        temp_v0_98 = *(s16 *)((s8 *) temp_v0_95 + 0x48);
                        var_a0_102 = 0;
                        if (temp_v0_98 < 5) {
                            var_a0_102 = 1;
                        } else {
                            temp_v0_250 = (u16) (temp_v0_98 - 0x32);
                            if (temp_v0_250 < 5U) {
                                var_a0_102 = 1;
                            }
                        }
                        if (var_a0_102 != 0) {
                            if (func_0020C014(temp_v0_54) != 0) {
                                if (sp10 == 0) {
                                    func_001F0E64((s8 *) (*(void **) var_s0_92) + 0x44, 8);
                                } else if (sp10 == terminal_state) {
                                    func_001F0E64((s8 *) (*(void **) var_s0_92) + 0x44, 0x24);
                                }
                            } else if (sp14 == 0) {
                                func_001F0E64((s8 *) (*(void **) var_s0_92) + 0x44, 8);
                            } else if (sp14 == terminal_state) {
                                func_001F0E64((s8 *) (*(void **) var_s0_92) + 0x44, 0x24);
                            }
                        }
                    }
                    var_s0_92 += 4;
                } while ((u32) var_s0_92 < (u32) temp_s1_93);
            }
            var_s3_49 += 1;
        } while (var_s3_49 < 0x14);
    }
    temp_a2_150 = D_801CE8BC;
    temp_v1_151 = (*(s32 *)((s8 *)(temp_a2_150) + (0x6048)));
    if (temp_v1_151 < 0x10) {
        D_801CEAAB = (u8) (temp_v1_151 * 0xC);
        return;
    }
    if (temp_v1_151 < var_s4_19) {
        (*(s32 *)((s8 *)(temp_a2_150) + (0x604C))) = (s32) ((*(s32 *)((s8 *)(temp_a2_150) + (0x604C))) | 1);
        return;
    }
    if (temp_v1_151 < (var_s4_19 + 0x10)) {
        var_v0_117 = temp_v1_151 - var_s4_19;
        temp_v1_151 = 0x10;
        temp_v1_151 -= var_v0_117;
        var_a1_179 = temp_v1_151 * 0xC;
        var_a1_179 &= (s32) ~var_a1_179 >> 0x1F;
        if (var_a1_179 >= 0x100) {
            var_a1_179 = 0xFF;
        }
        temp_a0_70 = &D_801D0758;
        temp_v1_151 = *(u16 *)temp_a0_70;
        temp_v1_151 += 0x11;
        var_v0_117 = *(s32 *)((s8 *)temp_a2_150 + 0x6048) - var_s4_19;
        temp_v1_151 -= var_v0_117;
        *(u16 *)temp_a0_70 = (u16) temp_v1_151;
        temp_v1_151 = *(u16 *)&D_801D075C;
        temp_v1_151 -= 0x11;
        var_v0_117 = *(s32 *)((s8 *)temp_a2_150 + 0x6048) - var_s4_19;
        temp_v1_151 += var_v0_117;
        D_801D075C = (u16) temp_v1_151;
        flags_604C = temp_a2_150->flags_604C.read;
        D_801CEAAB = (u8) var_a1_179;
        flags_604C &= 0xFFFFFFFEU;
        temp_a2_150->flags_604C.write = flags_604C;
        return;
    }
    if (((func_0020BF8C() != 0) || (func_0020BF7C() != 0)) && ((*(u8 *)((s8 *)D_801CE8BC + (0x6087))) == 1)) {
        var_s1_224 = 0;
        do {
            temp_v0_226 = func_0020C478(var_s1_224);
            if (temp_v0_226 != 0) {
                if (func_0020C014(temp_v0_226) != 0) {
                    (*(s16 *)((s8 *)(temp_v0_226) + (0x20))) = 0;
                }
            }
            var_s1_224 += 1;
        } while (var_s1_224 < 0x14);
        (*(u16 *)((s8 *)D_801CE8BC + (0x606A))) = func_002224F4();
    }
    temp_v0_250 = (*(u16 *)((s8 *)D_801CE8BC + (0x606A)));
    var_a0_252 = 4;
    if (temp_v0_250 != 0) {
        if (!(temp_v0_250 & 0x800)) {
            var_a0_252 = 0xA;
        }
    }
    func_0020BD5C(var_a0_252);
}
