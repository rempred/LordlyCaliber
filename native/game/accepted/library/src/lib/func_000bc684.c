typedef unsigned char u8;
typedef unsigned short u16;

typedef int (*EquipmentFunction)(u8, u8);

extern int func_0016B6FC(int);
extern EquipmentFunction D_801EF288[];

void func_000bc684(void)
{
    EquipmentFunction *functions;
    u8 classIndex;
    int index;
    int value;

    index = 0;
    functions = D_801EF288;
    classIndex = (*(u8 **)0x80196AF8)[0x5E9];
    do {
        value = (*functions)(classIndex, classIndex) & 0xFFFF;
        if (value != 0) {
            value = func_0016B6FC(value);
        } else {
            value = 0xFFFF;
        }
        functions++;
        *(u16 *)(*(u8 **)0x80196AF8 + 0x1BEE + (index * 2)) = value;
        index++;
    } while (index < 4);
}
