typedef unsigned char u8;

extern u8 D_801976DC;
extern char *D_801CE8BC;
extern u8 D_80197207[];
extern int *D_801CE8C0;

void func_001F1F20(char *scene);
void func_0020DB10(int selector);

void func_001F2134(int selector)
{
    char *scene = D_801CE8BC;
    int *state;

    *(int *)(scene + 0x1C0) = D_80197207[D_801976DC * 25];
    func_001F1F20(scene);

    state = D_801CE8C0;
    state[0] = 0;
    state[1] = 0;
    state[2] = 0;
    func_0020DB10(selector);
}
