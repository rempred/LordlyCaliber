typedef signed char s8;
typedef signed short s16;
typedef signed int s32;

s32 func_00047a94(s32 arg0);
void func_00047dd8(s32 arg0);

extern s16 D_80197B06;
extern volatile s32 D_80197B0C;

void func_0004813C(s32 arg0)
{
    s32 request;
    s32 active;

    active = D_80197B06;
    arg0 = (s16)arg0;
    *(s32 *)0x80197B10 = arg0;
    *(s32 *)0x80197B0C = arg0;
    *(s8 *)0x80197B03 = 0;
    if (active != 0) {
        request = D_80197B0C;
        *(s8 *)0x80197B02 = 0;
        if (request & 1) {
            *(s8 *)0x80197B00 = 0;
        } else if (request & 2) {
            *(s8 *)0x80197B00 = 1;
        } else if (request & 4) {
            *(s8 *)0x80197B00 = 2;
        } else if (request & 8) {
            *(s8 *)0x80197B00 = 3;
        } else if (request & 0x10) {
            *(s8 *)0x80197B00 = 4;
        } else if (request & 0x20) {
            *(s8 *)0x80197B00 = 5;
        }
        *(s8 *)0x80197B01 = 0;
        *(s16 *)0x80197B04 = 0;
        func_00047a94(arg0);
    } else {
        *(s16 *)0x80197B04 = 0;
        func_00047dd8(arg0);
    }
}
