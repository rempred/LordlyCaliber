typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
extern u16 D_800AA010;
extern u32 D_800AA014;
void bcopy(const void *src, void *dst, s32 len);
void func_0001b240(u8 *companyCode, u8 *gameCode) {
    bcopy(companyCode, &D_800AA010, 2);
    bcopy(gameCode, &D_800AA014, 4);
}
