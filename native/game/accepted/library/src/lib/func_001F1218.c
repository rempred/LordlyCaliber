typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;

extern u8 func_00043d70(int, int);
extern int func_00204F34(int, int, int, int, int, int, u8 *, u8 *, u8 *);
extern int func_002050AC(int, int, int, u8 *, u8 *, u8 *);
extern void func_00208DC8(const void *, int);
extern int func_0020C014(void *);
extern void func_0020C4B8(int, int, int, int *, int);
extern void func_80093380(void *, unsigned int);
extern void *func_0002CD70(void *, int, unsigned int);
extern int rand(void);
extern const u8 g_func_0015DF10_match[];
extern const u8 g_func_0015DF10_default[];
extern u16 D_801CF3F0[];
extern u16 D_801CF3F2[];
extern u16 D_801CF3F4[];
extern u16 D_801CEF10[];
extern u16 D_801CEF12[];
extern u8 D_80193BD4[];
extern u8 D_80195574[];
extern u8 D_801976DC;
extern u8 D_801976E8;

typedef struct {
    s32 field00;
    s16 field04, field06, field08, field0A, field0C, field0E, field10;
    u8 field12[16];
    s8 field22[16];
    u8 field32[4], field36[4], field3A[4], field3E[4], field42[4];
    u8 field46, field47, field48, pad49[3];
    void *field4C;
} Func001F1218State;

void func_001F1218(Func001F1218State *state, int arg1, int arg2, int arg3, int arg4)
{
    u8 byte0, byte1, byte2;
    int position;
    int code;
    u32 randomFirst;
    u32 randomPart;
    s16 selection;
    int index;
    int value;
    int classValue;
    int sum;
    void *positionChild;
    const void *recipient;
    void *child;
    void *parent;
    unsigned int i;
    u8 *cursor;

    if (state->field08 <= 0) {
        do {
            u16 next = (u16)state->field06 + 1;
            state->field06 = next;
            if (arg1 == 0x100) {
                code = func_002050AC(state->field00, state->field04, (s16)next,
                                    &byte0, &byte1, &byte2);
            } else {
                child = state->field4C;
                parent = *(void **)((u8 *)child + 0x40);
                if (*(void **)((u8 *)parent + 0x18) == child ||
                    *(void **)((u8 *)parent + 0x1C) == child) {
                    code = func_002050AC(state->field00, state->field04, (s16)next,
                                        &byte0, &byte1, &byte2);
                } else {
                    code = func_00204F34(arg1, arg2, arg4, arg3,
                                        state->field04, state->field06,
                                        &byte0, &byte1, &byte2);
                }
            }
            switch (code & 0xFF) {
            case 1: {
                int first = byte0;
                int second = byte1;
                state->field0A = first;
                state->field08 = second;
                break;
            }
            case 21: {
                int first = byte0 + (byte1 << 8);
                int second = byte2;
                state->field0A = first;
                state->field08 = second;
                break;
            }
            case 3:
                state->field08 = byte0;
                break;
            case 0:
                if (state->field46) {
                    --state->field46;
                    state->field04 = state->field32[state->field46];
                    state->field06 = state->field36[state->field46];
                } else {
                    selection = 0;
                    if (state->field48 & 2) {
                        randomFirst = rand();
                        randomPart = rand();
                        selection = (((randomFirst << 18) & 0x0C000000) |
                                (randomPart << 15) | (u32)rand()) % 5;
                    }
                    if (state->field48 & 1) selection += 50;
                    state->field04 = selection;
                    state->field06 = -1;
                    state->field08 = 0;
                    state->field10 = 0;
                    state->field0E = 0;
                    state->field0C = 0;
                    state->field46 = 0;
                    state->field47 = 0;
                    func_0002CD70(state->field12, 255, 16);
                    func_80093380(state->field22, 16);
                }
                break;
            case 17:
                func_00208DC8(g_func_0015DF10_match,
                             D_801CF3F0[(byte0 << 8) + byte1]);
                break;
            case 19:
                func_00208DC8(g_func_0015DF10_default,
                             D_801CF3F0[(byte0 << 8) + byte1]);
                break;
            case 18:
            case 20:
                positionChild = state->field4C;
                if (positionChild) {
                    func_0020C4B8(*(s16 *)((u8 *)positionChild + 0x1C),
                                 *(s16 *)((u8 *)positionChild + 0x1E),
                                 *(s16 *)((u8 *)positionChild + 0x20), &position, 0);
                    if ((u32)((u16)state->field04 - 29) < 2) {
                        byte0 = 0;
                        if (arg2 == 0x33) byte1 = 6;
                        else if (arg2 == 0x2F) byte1 = 11;
                        else if ((u32)(arg2 - 0x4E) < 3) byte1 = 10;
                        else if ((u32)(arg2 - 0x38) < 13) byte1 = 8;
                        else {
                            switch (func_00043d70(arg1 & 255, arg2 & 255) & 255) {
                            case 0:
                            case 1:
                            case 4:
                                if (func_0020C014(*(void **)((u8 *)state->field4C + 0x40)))
                                    classValue = D_801976DC < 30;
                                else classValue = D_801976E8 < 30;
                                if (classValue)
                                    classValue = D_80193BD4[*(u8 *)((u8 *)*(void **)((u8 *)state->field4C + 0x40) + 0xF6) * 56];
                                else
                                    classValue = D_80195574[*(u8 *)((u8 *)*(void **)((u8 *)state->field4C + 0x40) + 0xF6) * 52];
                                if (classValue == 0) byte1 = 4;
                                else byte1 = 5;
                                break;
                            case 2:
                                byte1 = 9;
                                break;
                            default:
                                byte1 = 7;
                                break;
                            }
                        }
                    }
                    index = (byte0 << 8) + byte1;
                    if ((u32)(index - 0x24E) < 0xAB) {
                        recipient = (code & 255) == 18 ? g_func_0015DF10_match : g_func_0015DF10_default;
                        if (position >= 213) value = D_801CF3F0[index];
                        else if (position < 106) value = D_801CF3F4[index];
                        else value = D_801CF3F2[index];
                    } else {
                        recipient = (code & 255) == 18 ? g_func_0015DF10_match : g_func_0015DF10_default;
                        if (position < 160) value = *(u16 *)((u8 *)(index * 4 + 0x801D0000U) - 0x10F0);
                        else value = D_801CEF12[index * 2];
                    }
                } else {
                    index = (byte0 << 8) + byte1;
                    recipient = (code & 255) == 18 ? g_func_0015DF10_match : g_func_0015DF10_default;
                    value = D_801CEF10[index * 2];
                }
                func_00208DC8(recipient, value);
                break;
            case 2: {
                u8 first = byte0;
                u8 second = byte1;
                state->field10 -= (s8)first;
                state->field0E -= (s8)second;
                break;
            }
            case 12: {
                u8 first = byte0;
                u8 second = byte1;
                u8 third = byte2;
                state->field10 -= (s8)first;
                state->field0E += (s8)second;
                state->field0C += (s8)third;
                break;
            }
            case 13:
                if (byte0 == 255) func_0002CD70(state->field12, byte1, 16);
                else state->field12[byte0] = byte1;
                break;
            case 16:
                if (byte0 == 255) func_0002CD70(state->field22, byte1, 16);
                else state->field22[byte0] = byte1;
                break;
            case 15:
                state->field32[state->field46] = state->field04;
                state->field36[state->field46] = state->field06;
                ++state->field46;
            case 5: {
                int first = byte0;
                state->field06 = -1;
                state->field04 = first;
                break;
            }
            case 14:
                cursor = (u8 *)state + state->field47;
                if (state->field47 && cursor[0x39] == state->field04 &&
                    cursor[0x3D] == state->field06) {
                    if (--cursor[0x41] == 0) {
                        --state->field47;
                        break;
                    }
                } else {
                    if (!byte1) break;
                    state->field3A[state->field47] = state->field04;
                    state->field3E[state->field47] = state->field06;
                    state->field42[state->field47] = byte1;
                    ++state->field47;
                }
            case 4:
                state->field06 = byte0 - 1;
                break;
            }
        } while (state->field08 <= 0);
    }
    i = 0;
    do {
        cursor = (u8 *)state + i;
        sum = cursor[0x12] + (s8)cursor[0x22];
        if (sum < 0) cursor[0x12] = 0;
        else if (sum >= 256) cursor[0x12] = 255;
        else cursor[0x12] = sum;
        ++i;
    } while (i < 16);
    state->field08 -= 2;
}
