extern int D_80197B10;
extern int D_80197B0C;
extern unsigned char D_80197B03;
extern unsigned char D_80197B00;
extern unsigned char D_80197B01;
extern unsigned char D_80197B02;
extern unsigned short D_80197B04;

void func_80171B94(void);

void func_0004820c(short state, int slot0, int slot1)
{
    D_80197B10 = state;
    D_80197B0C = state;
    D_80197B03 = 1;
    D_80197B00 = slot0;
    D_80197B01 = slot1;
    D_80197B02 = 0;
    D_80197B04 = 0;
    func_80171B94();
}
