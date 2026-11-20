typedef unsigned char u8;
typedef struct {
    signed char *name;
    u8 kind;
    u8 element;
    u8 effect1[4];
    u8 effect2[4];
} AttackData;
extern AttackData D_8018AA7C[];
signed char *func_00201430(u8 i) {
    return D_8018AA7C[i + 0x37].name;
}
