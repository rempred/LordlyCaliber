typedef unsigned char u8;
typedef signed short s16;
typedef struct {
    s16 h0;
    s16 h2;
    u8 b4;
    u8 b5;
    u8 b6;
    u8 b7;
    u8 b8;
    u8 b9;
    u8 bA;
    u8 bB;
} StFC10;
extern StFC10 *D_8018FC10;
s16 func_002839C4(void) {
    if (D_8018FC10 != ((void *)0)) {
        return *(s16 *)D_8018FC10;
    }
    return 0;
}
