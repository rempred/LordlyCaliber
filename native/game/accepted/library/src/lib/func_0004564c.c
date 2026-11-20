typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef struct {
    signed char *name;
    u8 category;
    u8 element;
    u8 rank;
    u16 price;
    u8 str;
    u8 intl;
    u8 agi;
    u8 dex;
    u8 vit;
    u8 men;
    s8 luck;
    s8 resist[7];
    u8 growth[2];
    u8 flags;
    u8 inflict;
    u8 protect;
} ItemData;
extern ItemData D_8018C40C[];
extern u16 D_80193C1A;
s8 func_0004564c(u16 id) {
    u8 v;
    if (id == 132) {
        v = D_80193C1A / 20;
    } else {
        v = D_8018C40C[id].men;
    }
    return v;
}
