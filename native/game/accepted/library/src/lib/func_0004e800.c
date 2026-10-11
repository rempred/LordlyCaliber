extern unsigned char D_8018FC18;
extern unsigned short D_800C4C26;

void func_80198D08(void);

void func_0004e800(void)
{
    D_8018FC18 = 0;
    func_80198D08();
    D_800C4C26 = 0x800D;
}
