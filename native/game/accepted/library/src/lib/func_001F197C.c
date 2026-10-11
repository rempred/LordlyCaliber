typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;

typedef struct {
    int field00;
    u16 field04;
    s16 field06, field08, field0A, field0C, field0E, field10;
    u8 field12[16];
    s8 field22[16];
    u8 field32[4], field36[4], field3A[4], field3E[4], field42[4];
    u8 field46, field47;
} Func001F197CState;

extern int func_00204F34(int, int, int, int, int, int, u8 *, u8 *, u8 *);
extern int func_0020BFF8(void *);
extern int func_0020C034(void *);
extern void *func_0002CD70(void *, int, unsigned int);
extern void func_80093380(void *, unsigned int);

int func_001F197C(void *object, Func001F197CState *state, int *out)
{
    u8 byte0, byte1, byte2;
    int count;
    int total;
    int mode;
    u8 *cursor;

    count = 0;
    total = 0;
    if (out) *out = 0;
    if ((s16)((s16)state->field04 % 50) != 0) {
        do {
            ++state->field06;
            mode = func_0020C034(object);
            switch (func_00204F34(*(int *)((u8 *)object + 0x48),
                                  *(int *)((u8 *)object + 0x4C), mode,
                                  func_0020BFF8(object), (s16)state->field04,
                                  state->field06, &byte0, &byte1, &byte2) & 255) {
            case 1:
                total += byte1;
                state->field0A = byte0;
                break;
            case 21:
                total += byte2;
                state->field0A = byte0 + (byte1 << 8);
                break;
            case 4:
                state->field04 = 0;
                state->field06 = -1;
                break;
            case 5: {
                u8 first = byte0;
                state->field06 = -1;
                state->field08 = 0;
                state->field10 = 0;
                state->field0E = 0;
                state->field0C = 0;
                state->field46 = 0;
                state->field47 = 0;
                state->field04 = first;
                func_0002CD70(state->field12, 255, 16);
                func_80093380(state->field22, 16);
                break;
            }
            case 3:
                total += byte0;
                break;
            case 11:
                if (out) {
                    out[count] = total / 2;
                    ++count;
                }
                break;
            case 0:
                if (state->field46) {
                    --state->field46;
                    state->field04 = state->field32[state->field46];
                    state->field06 = state->field32[state->field46];
                } else {
                    state->field06 = -1;
                    state->field04 = (s16)((s16)state->field04 / 50) * 50;
                }
                break;
            case 15: {
                u8 first;
                state->field32[state->field46] = state->field04;
                state->field36[state->field46] = state->field06;
                ++state->field46;
                first = byte0;
                state->field06 = -1;
                state->field04 = first;
                break;
            }
            case 14:
                cursor = (u8 *)state + state->field47;
                if (state->field47 && cursor[0x39] == (s16)state->field04 &&
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
                state->field06 = byte0 - 1;
                break;
            }
        } while ((s16)((s16)state->field04 % 50) != 0);
    }
    if (out) {
        if (count == 0) {
            *out = 0;
            count = 1;
        }
        out[count] = -1;
    }
    return total / 2;
}
